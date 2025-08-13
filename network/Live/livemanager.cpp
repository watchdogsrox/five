//
// livemanager.cpp
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#include <algorithm>
#if __WIN32PC
#include <tchar.h>
#endif

#if RSG_DURANGO
#include "rline/durango/rlxbl_interface.h"
#endif

// Rage headers
#include "bank/bkmgr.h"
#include "diag/seh.h"
#include "file/tcpip.h"
#include "input/dongle.h"
#include "net/net.h"
#include "net/nethardware.h"
#include "net/netsocket.h"
#include "net/task.h"
#include "rline/presence/rlpresencequery.h"
#include "rline/rlnp.h"
#include "rline/rlprivileges.h"
#include "rline/rlsessionfinder.h"
#include "rline/rltitleid.h"
#include "rline/rlsystemui.h"
#include "rline/ros/rlroscommon.h"
#include "rline/ros/rlros.h"
#include "rline/socialclub/rlsocialclubcommon.h"
#include "rline/scmatchmaking/rlscmatchmanager.h"
#include "rline/scmembership/rlscmembership.h"
#include "rline/youtube/rlyoutube.h"
#include "system/appcontent.h"
#include "system/param.h"
#include "system/language.h"
#include "system/simpleallocator.h"
#include "system/sparseallocator.h"

#if RSG_DURANGO
#include "rline/durango/rlxbl_interface.h"
#endif

// FrameWork Headers
#include "fwnet/netchannel.h"
#include "fwnet/netutils.h"
#include "fwnet/netscgameconfigparser.h"
#include "fwnet/netprofanityfilter.h"
#include "fwrenderer/renderthread.h"
#include "fwscene/stores/mapdatastore.h"
#include "fwscene/stores/maptypesstore.h"

// Network Headers
#include "livemanager.h"
#include "livep2pkeys.h"
#include "frontend/CFriendsMenu.h"
#include "frontend/Store/StoreScreenMgr.h"
#include "Network/Network.h"
#include "Network/xlast/Fuzzy.schema.h"
#include "network/xlast/presenceutil.h"
#include "Network/NetworkInterface.h"
#include "Network/Live/NetworkRichPresence.h"
#include "Network/Sessions/NetworkSession.h"
#include "Network/Sessions/NetworkVoiceSession.h"
#include "Network/Live/NetworkClan.h"
#include "Network/Live/PlayerCardDataManager.h"
#include "Network/xlast/presenceutil.h"
#include "Network/live/NetworkProfileStats.h"
#include "Network/live/NetworkSCPresenceUtil.h"
#include "Network/Shop/GameTransactionsSessionMgr.h"
#include "Network/Shop/NetworkShopping.h"
#include "Network/Shop/Catalog.h"
#include "Network/SocialClubServices/GamePresenceEventDispatcher.h"
#include "Network/SocialClubServices/GamePresenceEvents.h"
#include "Network/SocialClubServices/SocialClubCommunityEventMgr.h"
#include "Network/SocialClubServices/SocialClubEmailMgr.h"
#include "Network/SocialClubServices/SocialClubFeedMgr.h"
#include "Network/SocialClubServices/SocialClubFeedTilesMgr.h"
#include "Network/SocialClubServices/SocialClubNewsStoryMgr.h"
#include "Network/SocialClubServices/SocialClubInboxManager.h"
#include "Network/Cloud/CloudManager.h"
#include "network/Live/NetworkLegalRestrictionsManager.h"
#include "network/Cloud/VideoUploadManager.h"
#include "Network/Voice/NetworkVoice.h"

#if RSG_GEN9
#include "Network/live/NetworkLandingPageStatsMgr.h"
#endif

// Game Headers
#include "camera/CamInterface.h"
#include "camera/viewports/ViewportManager.h"
#include "event/EventGroup.h"
#include "event/EventNetwork.h"
#include "Peds/PlayerInfo.h"
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_autoload.h"
#include "Scene/ExtraContent.h"
#include "scene/FocusEntity.h"
#include "Scene/world/gameWorld.h"
#include "streaming/streamingengine.h"
#include "Control/gamelogic.h"
#include "System/controlMgr.h"
#include "system/pad.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "Frontend/PauseMenu.h"
#include "Frontend/WarningScreen.h"
#include "Frontend/SocialClubMenu.h"
#include "system/memvisualize.h"
#include "system/EntitlementManager.h"
#include "frontend/ProfileSettings.h"
#include "frontend/loadingscreens.h"
#include "frontend/VideoEditor/ui/Editor.h"
#include "frontend/landing_page/LandingPageConfig.h"
#include "control/replay/replay.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"

#if RSG_PROSPERO
#include "script/streamedscripts.h"
#endif

#if __BANK
#include "streaming/streamingdebug.h"
#include "fwscene/stores/mapdatastore.h"
#endif //__BANK

#if RSG_PC && !__NO_OUTPUT
namespace rage
{
	XPARAM(processinstance);
}
#endif

#if RSG_PC
#include "rline/rlpc.h"
#include "frontend/TextInputBox.h"
#endif // RSG_PC

#include "Network/Live/NetworkTelemetry.h"
#include "Stats/StatsMgr.h"
#include "Stats/StatsDataMgr.h"
#include "Stats/StatsInterface.h"
#include "text/messages.h"
#include "game/config.h"

#include "Network/Cloud/CloudManager.h"
#include "Network/Cloud/VideoUploadManager.h"
#include "Network/Cloud/UserContentManager.h"

#include "Network/Live/NetworkGamerAttributes.h"
#include "Network/Shop/NetworkGameTransactions.h"

NETWORK_OPTIMISATIONS()

PARAM(rlineHeapSize, "[live] Size of rline heap.");

#if RSG_PC || RSG_DURANGO
PARAM(rlineSparseAllocator, "[network] Use a sparse allocator for rline");
#endif

PARAM(useMarketplace, "[live] Activate marketplace.");
PARAM(inviteAssumeSend, "[live] On PS3 - will halt at the XMB when sending invites");
XPARAM(nonetwork);
NOSTRIP_XPARAM(uilanguage);

PARAM(netPrivilegesMultiplayer, "[network] Force privilege setting for multiplayer", "Default multiplayer privilege setting", "All", "");
PARAM(netPrivilegesUserContent, "[network] Force privilege setting for user content", "Default UGC privilege setting", "All", "");
PARAM(netPrivilegesVoiceCommunication, "[network] Force privilege setting for voice communication", "Default communication privilege setting", "All", "");
PARAM(netPrivilegesTextCommunication, "[network] Force privilege setting for text communication", "Default communication privilege setting", "All", "");
PARAM(netPrivilegesAgeRestricted, "[network] Force privilege setting for age restriction", "Default communication privilege setting", "All", "");
PARAM(netPrivilegesRos, "[network] Force privilege setting for Rockstar services", "Default communication privilege setting", "All", "");
PARAM(netPrivilegesSocialNetworkSharing, "[network] Force privilege setting for social network sharing", "Default communication privilege setting", "All", "");

#if RSG_ORBIS
NOSTRIP_PARAM(netFromLiveArea, "[network] Forces -FromLiveArea");
#endif

#if RSG_ORBIS
PARAM(netHasActivePlusEvent, "[net_live] PS4 only - Adds active PS+ event (bypassing need for event in event schedule)");
PARAM(netPlusEventEnabled, "[net_live] PS4 only - Enables PS+ event (bypassing need for tunable)");
PARAM(netPlusCustomUpsellEnabled, "[net_live] PS4 only - When PS+ event is active and enabled - enables custom upsell");
PARAM(netPlusPromotionEnabled, "[net_live] PS4 only - When PS+ event is active and enabled - enables PS+ promotion");
PARAM(netPlusEventPromotionEndGraceMs, "[net_live] PS4 only - When PS+ event is active and enabled - grace period to exit RDO when promotion ends");
#endif

#if NET_ENABLE_MEMBERSHIP_FUNCTIONALITY && RSG_PROSPERO
PARAM(netMembershipRefreshOnBackgroundExecutionExit, "[net_live] PS5 Only - Refresh membership status after exiting background execution");
#endif

#if RSG_XBOX
PARAM(netXblSessionTemplateName, "[net_live] Xbox Only - Session template name");
#endif

CompileTimeAssert((int) MAX_ACHIEVEMENTS >= (int) player_schema::Achievements::COUNT);

RAGE_DEFINE_SUBCHANNEL(net, live, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_live

#if RSG_NP
RAGE_DEFINE_SUBCHANNEL(net, realtime, DIAG_SEVERITY_DEBUG3, DIAG_SEVERITY_WARNING, DIAG_SEVERITY_ASSERT)
#define netRealtimeScriptDebug(fmt, ...)	RAGE_DEBUGF1(net_realtime, fmt, ##__VA_ARGS__)
#endif

// EJ: I brought these values over from MP3
#if __WIN32PC || RSG_ORBIS || RSG_DURANGO
const unsigned          CLiveManager::RLINE_HEAP_SIZE   = (1536*1024); // 1.5MB
#else
const unsigned          CLiveManager::RLINE_HEAP_SIZE   = (194*1024);
#endif

static u8              *s_RlineHeap = 0;
sysMemAllocator		   *CLiveManager::sm_RlineAllocator = 0;
netSocketManager	   *CLiveManager::sm_SocketMgr = nullptr;

//Presence manager
rlPresence::Delegate    CLiveManager::sm_PresenceDlgt(&CLiveManager::OnPresenceEvent);
richPresenceMgr         CLiveManager::sm_RichPresenceMgr;

//Ros
rlRos::Delegate				CLiveManager::sm_RosDlgt(&CLiveManager::OnRosEvent);
rlFriendsManager::Delegate	CLiveManager::sm_FriendDlgt(&CLiveManager::OnFriendEvent);
ServiceDelegate				CLiveManager::sm_ServiceDelegate(&CLiveManager::OnServiceEvent);

#if NET_ENABLE_MEMBERSHIP_FUNCTIONALITY
rlScMembership::Delegate	CLiveManager::sm_MembershipDlgt(&CLiveManager::OnMembershipEvent);

// true so that we always do this once
static const unsigned DEFAULT_EVALUATE_BENEFITS_INTERVAL = 1000 * 60 * 60 * 24; // 1 day
unsigned CLiveManager::sm_EvaluateBenefitsInterval = DEFAULT_EVALUATE_BENEFITS_INTERVAL;
unsigned CLiveManager::sm_LastEvaluateBenefitsTime = 0;
bool CLiveManager::sm_NeedsToEvaluateBenefits = true;
#endif

#if ENABLE_SINGLE_PLAYER_CLOUD_SAVES
rlPcEventDelegator::Delegate CLiveManager::sm_PcDlgt(&CLiveManager::OnPcEvent);
#endif

//Profile stats manager
CProfileStats CLiveManager::sm_ProfileStatsMgr;

//Invite Managers
InviteMgr CLiveManager::sm_InviteMgr;
PartyInviteMgr CLiveManager::sm_PartyInviteMgr;

//Friends Manager
bool CLiveManager::sm_bEvaluateFriendPeds;
rlFriendsPage CLiveManager::sm_FriendsPage;
rlFriendsPage CLiveManager::sm_FriendsReadBuf;
bool CLiveManager::sm_bFriendActivatedNeedsUpdate = false;
netStatus CLiveManager::sm_FriendStatus;
u32 CLiveManager::sm_FriendsInMatch = 0;

//Achievements Manager
AchMgr CLiveManager::sm_AchMgr;

#if RL_FACEBOOK_ENABLED
CFacebook CLiveManager::sm_Facebook;
#endif // #if RL_FACEBOOK_ENABLED

netSCGamerDataMgr CLiveManager::sm_scGamerDataMgr;
cCommerceManager CLiveManager::sm_Commerce;

#if RSG_PC
extern const char* ATTR_TRANSITION_INFO;
extern const char* ATTR_TRANSITION_IS_JOINABLE;
#endif

#if RSG_DURANGO
// the const / default makes it easier to test this setting (i.e. set this to true to test around the parameter being true on the IIS at boot)
const bool m_SignedInIntoNewProfileAfterSuspendMode_Default = false;
bool CLiveManager::sm_SignedInIntoNewProfileAfterSuspendMode = m_SignedInIntoNewProfileAfterSuspendMode_Default;
#endif

bool         CLiveManager::sm_bOfflineInvitePending = false;
bool         CLiveManager::sm_bSubscriptionTelemetryPending = false;
bool         CLiveManager::sm_bMembershipTelemetryPending = false;
bool         CLiveManager::sm_bInitialisedSession = false;

bool		 CLiveManager::sm_bIsOnlineRos[RL_MAX_LOCAL_GAMERS];
bool		 CLiveManager::sm_bHasValidCredentials[RL_MAX_LOCAL_GAMERS];
bool		 CLiveManager::sm_bHasValidRockstarId[RL_MAX_LOCAL_GAMERS];
bool         CLiveManager::sm_bHasCredentialsResult[RL_MAX_LOCAL_GAMERS];
bool		 CLiveManager::sm_bConsiderForPrivileges[RL_MAX_LOCAL_GAMERS];
bool		 CLiveManager::sm_bPermissionsValid[RL_MAX_LOCAL_GAMERS];

unsigned     CLiveManager::sm_OnlinePrivilegePromotionEndedTime[RL_MAX_LOCAL_GAMERS];

bool		 CLiveManager::sm_bHasOnlinePrivileges[RL_MAX_LOCAL_GAMERS];
bool		 CLiveManager::sm_bIsRestrictedByAge[RL_MAX_LOCAL_GAMERS];
bool		 CLiveManager::sm_bHasUserContentPrivileges[RL_MAX_LOCAL_GAMERS];
bool		 CLiveManager::sm_bHasVoiceCommunicationPrivileges[RL_MAX_LOCAL_GAMERS];
bool		 CLiveManager::sm_bHasTextCommunicationPrivileges[RL_MAX_LOCAL_GAMERS];

#if RSG_ORBIS
bool		 CLiveManager::sm_bFlagIpReleasedOnGameResume = true; 
#endif

static const int DEFAULT_LARGE_CREW_THRESHOLD = 100;
int	 CLiveManager::sm_LargeCrewThreshold = DEFAULT_LARGE_CREW_THRESHOLD;

#if RSG_XBOX
bool		CLiveManager::sm_bPendingPrivilegeCheckForUi;
rlPrivileges::PrivilegeTypes CLiveManager::sm_PendingPrivilegeType;
int			CLiveManager::sm_PendingPrivilegeLocalGamerIndex;
#endif

bool		 CLiveManager::sm_bHasSocialNetSharePriv[RL_MAX_LOCAL_GAMERS];

bool         CLiveManager::sm_IsInitialized = false;
unsigned     CLiveManager::sm_LastUpdateTime;

#if RSG_NP
unsigned CLiveManager::sm_RealtimeMultiplayerScriptFlags = RealTimeMultiplayerScriptFlags::RTS_None;
#if !__NO_OUTPUT
unsigned CLiveManager::sm_PreviousRealtimeMultiplayerScriptFlags = RealTimeMultiplayerScriptFlags::RTS_None;
#endif
#endif

CompileTimeAssert(UnacceptedInvite::RL_MAX_INVITE_SALUTATION_CHARS == RL_MAX_INVITE_SALUTATION_CHARS);

SocialClubMgr		CLiveManager::sm_SocialClubMgr;
//NetworkClan			CLiveManager::sm_NetworkClan;
CNetworkCrewDataMgr	CLiveManager::sm_NetworkClanDataMgr;

netStatus CLiveManager::sm_AddFriendStatus;

CLiveManager::FindGamerTask* CLiveManager::sm_FindGamerTask = NULL;
CLiveManager::GetGamerStatusQueue* CLiveManager::sm_GetGamerStateQueue = NULL;
CLiveManager::GetGamerStatusTask* CLiveManager::sm_GetGamerStatusTask = NULL;

#if RSG_XBOX || RSG_PC
CGamerTagFromHandle CLiveManager::sm_FindGamerTag;
BANK_ONLY( static bool s_WidgetFindGamerTag = false; )
#endif

#if RSG_DURANGO || RSG_ORBIS
CDisplayNamesFromHandles CLiveManager::sm_FindDisplayName;
BANK_ONLY( static int s_WidgetDisplayNameRequestId[10] = {CDisplayNamesFromHandles::INVALID_REQUEST_ID, CDisplayNamesFromHandles::INVALID_REQUEST_ID,
														  CDisplayNamesFromHandles::INVALID_REQUEST_ID, CDisplayNamesFromHandles::INVALID_REQUEST_ID,
														  CDisplayNamesFromHandles::INVALID_REQUEST_ID, CDisplayNamesFromHandles::INVALID_REQUEST_ID,
														  CDisplayNamesFromHandles::INVALID_REQUEST_ID, CDisplayNamesFromHandles::INVALID_REQUEST_ID,
														  CDisplayNamesFromHandles::INVALID_REQUEST_ID, CDisplayNamesFromHandles::INVALID_REQUEST_ID}; )
#endif // RSG_DURANGO || RSG_ORBIS

#if RSG_XDK
bool CLiveManager::sm_OverallReputationIsBadNeedsRefreshed = false;
bool CLiveManager::sm_OverallReputationIsBadRefreshing = false;
bool CLiveManager::sm_OverallReputationIsBad = false;
rlXblUserStatiticsInfo CLiveManager::sm_OverallReputationIsBadStatistic;
netStatus CLiveManager::sm_StatisticsStatus;
#endif

#if RSG_XBOX
PlayerList CLiveManager::sm_AvoidList; 
bool CLiveManager::sm_bAvoidListNeedsRefreshed = false;
bool CLiveManager::sm_bAvoidListRefreshing = false;
bool CLiveManager::sm_bHasAvoidList = false;
netStatus CLiveManager::sm_AvoidListStatus; 
#endif

#if RSG_ORBIS
bool CLiveManager::sm_bPendingAppLaunchScriptEvent = false;
unsigned CLiveManager::sm_nPendingAppLaunchFlags = 0;
#endif

#if ENABLE_SINGLE_PLAYER_CLOUD_SAVES
rgsc::ICloudSaveManifest* CLiveManager::m_CloudSaveManifest = NULL;
rgsc::ICloudSaveManifestV2* CLiveManager::m_CloudSaveManifestV2 = NULL;
#endif

bool CLiveManager::sm_bSuspendActionPending = false;
bool CLiveManager::sm_bResumeActionPending = false;
bool CLiveManager::sm_bGameWasSuspended = false;
unsigned CLiveManager::sm_LastResumeTimestamp = 0;
unsigned CLiveManager::sm_LastBrowserUpsellRequestTimestamp = 0;

#if NET_ENABLE_MEMBERSHIP_FUNCTIONALITY
bool CLiveManager::sm_HasShownMembershipWelcome = false;
bool CLiveManager::sm_SuppressMembershipForScript = false;
bool CLiveManager::sm_HasCachedMembershipReceivedEvent = false;
rlScMembershipStatusReceivedEvent CLiveManager::sm_MembershipReceivedEvent(RL_INVALID_GAMER_INDEX, rlScMembershipInfo());
bool CLiveManager::sm_HasCachedMembershipChangedEvent = false;
rlScMembershipStatusChangedEvent CLiveManager::sm_MembershipChangedEvent(RL_INVALID_GAMER_INDEX, rlScMembershipInfo(), rlScMembershipInfo());
#endif

#if RSG_XBOX
rlGamerHandle CLiveManager::sm_SignedOutHandle;
#endif

bool CLiveManager::sm_PostponeProfileChange = false;
int CLiveManager::sm_ProfileChangeActionCounter = 0;

///////////////////////////////////////////////////////////////////////////////
//  CLiveManager
///////////////////////////////////////////////////////////////////////////////

NetworkClan& CLiveManager::GetNetworkClan()
{
	return NetworkClanInst::GetInstance(); 
}
//---------------------------------------------------------------------------
//	Helper function to validate account settings (trying to avoid info leaks
//	Courtesy Tom Shepherd R*SD
//---------------------------------------------------------------------------
#if !__FINAL
void CheckDEVAccount()
{

}
#endif // !__FINAL

//------------------------------------------------------------

#if !__FINAL
static const unsigned short SOCKET_PORT = 0x0ACE;
#else
static const unsigned short SOCKET_PORT = 0x1A10;
#endif // !__FINAL

bool CLiveManager::Init(const unsigned initMode)
{
	gnetDebug1("Init");

	//@@: range CLIVEMANAGER_INIT {
	bool success = true;

	if(initMode == INIT_CORE && !sm_IsInitialized)
	{
		// flatten ROS cache values
		for(unsigned i = 0; i < RL_MAX_LOCAL_GAMERS; i++)
		{
			sm_bIsOnlineRos[i] = false;
			sm_bHasValidCredentials[i] = false;
			sm_bHasValidRockstarId[i] = false;
            sm_bHasCredentialsResult[i] = false;
            sm_bPermissionsValid[i] = false;

			sm_OnlinePrivilegePromotionEndedTime[i] = 0;

			// privilege
			sm_bConsiderForPrivileges[i] = false;
			sm_bHasOnlinePrivileges[i] = false;
			sm_bIsRestrictedByAge[i] = false;
			sm_bHasUserContentPrivileges[i] = false;
			sm_bHasVoiceCommunicationPrivileges[i] = false;
			sm_bHasTextCommunicationPrivileges[i] = false;
		}

		// we have to create the network here rather than in the CNetwork init as LiveManager
		// uses the network heap first
		//@@: location CLIVEMANAGER_INIT_CREATENETWORKHEAP
		CNetwork::CreateNetworkHeap();

		// network debug needs to be initialised here rather than in the CNetwork init so heap memory usage is tracked
		NetworkDebug::Init();

		CNetwork::UseNetworkHeap(NET_MEM_RLINE_HEAP);
		//@@: range CLIVEMANAGER_INIT_INNER {

		unsigned nRlineHeapSize = CLiveManager::RLINE_HEAP_SIZE;
#if !__FINAL
		PARAM_rlineHeapSize.Get(nRlineHeapSize);
#endif
		s_RlineHeap       = rage_new u8[nRlineHeapSize];

#if !__FINAL && (RSG_PC || RSG_DURANGO)
		if(PARAM_rlineSparseAllocator.Get())
		{
			gnetDebug1("-rlineSparseAllocator enabled, using a sparse allocator for rline allocations");
			sm_RlineAllocator = rage_new sysMemSparseAllocator(nRlineHeapSize, sysMemSimpleAllocator::HEAP_NET);
		}
		else
#endif
		{
			gnetDebug1("using a simple allocator for rline allocations");
			sm_RlineAllocator = rage_new sysMemSimpleAllocator(s_RlineHeap, nRlineHeapSize, sysMemSimpleAllocator::HEAP_NET);
		}

		sm_SocketMgr = rage_new netSocketManager();
		if (!gnetVerify(sm_SocketMgr))
		{
			return false;
		}

		sm_RlineAllocator->SetQuitOnFail(false);
		CNetwork::StopUsingNetworkHeap();

#if RAGE_TRACKING
		diagTracker* t = diagTracker::GetCurrent();
		if (t && sysMemVisualize::GetInstance().HasNetwork())
		{
			t->InitHeap("Live Manager", s_RlineHeap, nRlineHeapSize);
		}
#endif // RAGE_TRACKING

		success = false;

		unsigned short port = SOCKET_PORT;

#if RSG_PC && !__NO_OUTPUT
		// support multiple instance of the game running on the same PC during development.
		u32 processInstance = 0;
		if (PARAM_processinstance.Get(processInstance))
		{
			if(gnetVerify(processInstance > 0))
			{
				port = 0;
			}
		}
#endif

		sm_SocketMgr->Init(sm_RlineAllocator, NETWORK_CPU_AFFINITY, port);

		//Init rage network
		netInit(sm_RlineAllocator, sm_SocketMgr);

		netP2pCrypt::HmacKeySalt p2pKeySalt;
		if (gnetVerify(CLiveP2pKeys::GetP2pKeySalt(p2pKeySalt)))
		{
			CNetwork::GetConnectionManager().SetP2pKeySalt(p2pKeySalt);
		}

#if RSG_ORBIS
		static const SceNpCommunicationId npCommId =
		{
			{'N','P','W','R','0','6','2','2','1'},
			'\0',
			0,
			0
		};

		static const SceNpCommunicationPassphrase npCommPassphrase = 
		{
			{
				0xd0,0xad,0x39,0x3a,0x23,0x4d,0xbd,0x02,
					0x2a,0x0b,0x71,0x98,0xde,0x51,0xb9,0x20,
					0x77,0x04,0xb4,0xf3,0xc8,0xac,0xda,0x95,
					0xc7,0xc4,0x7c,0xc0,0x80,0x3d,0x36,0x4f,
					0x9a,0xb5,0x34,0x03,0x84,0x72,0x5a,0x43,
					0x28,0x8b,0xc9,0x09,0x7a,0x4a,0x9b,0xa1,
					0xd5,0x84,0xa6,0x9a,0xcf,0xe5,0xc6,0xb2,
					0x6b,0xa0,0xa3,0xd9,0x34,0xc0,0xee,0x4b,
					0x9b,0xd6,0xc1,0xc8,0xd9,0xd4,0xcc,0x6c,
					0xe2,0xac,0x7e,0x0b,0xee,0xf2,0x14,0x88,
					0x7e,0xcb,0x36,0x50,0x17,0x3b,0x8d,0x90,
					0x39,0xdc,0x00,0x9b,0x24,0x1a,0x96,0x29,
					0x05,0x52,0xc5,0x48,0x7a,0x58,0xb3,0x85,
					0x6d,0xb2,0xe8,0x3f,0x5b,0x1a,0xa0,0x53,
					0x51,0xda,0xd5,0xaa,0x06,0x44,0x2f,0xb0,
					0xbb,0x4a,0x2e,0x5b,0xbc,0xf2,0xfa,0x71
			}
		};

		static const SceNpCommunicationSignature npCommSig = 
		{
			{
				0xb9,0xdd,0xe1,0x3b,0x01,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0xd8,0x51,0xa6,0x89,
					0xa2,0x5c,0x4a,0x36,0x82,0xfb,0x27,0x18,
					0x8c,0xdf,0x06,0xf2,0x9e,0xd6,0xc6,0xfc,
					0x06,0x6f,0x1f,0x20,0x53,0x63,0x42,0x90,
					0x69,0x41,0xc6,0x87,0x6f,0xef,0x46,0xb4,
					0xec,0xcd,0x9c,0xa7,0xa0,0xdb,0x82,0x1c,
					0x45,0xb5,0xb2,0xb9,0xb4,0xb1,0xd8,0x23,
					0xb2,0x16,0x5b,0xd7,0x5b,0xc1,0xea,0xe0,
					0xa2,0xe9,0xed,0xea,0x7f,0x62,0x9f,0x88,
					0xbd,0x36,0x7d,0xfd,0x73,0x2e,0xc7,0x55,
					0x63,0xcd,0x6f,0x24,0xcc,0xda,0x0c,0x7c,
					0x41,0xab,0x8b,0xe7,0x8c,0xf5,0xaf,0x94,
					0x63,0xd9,0xe3,0x91,0x5f,0x19,0xe3,0xb2,
					0x19,0x62,0x03,0x26,0x9d,0xd8,0x26,0xff,
					0xdb,0x1c,0x74,0x1e,0x5b,0x3f,0x9a,0x5e,
					0x9e,0x89,0xd9,0x76,0x28,0x32,0x7f,0x53,
					0xa5,0x7b,0xb1,0xf4,0x2a,0x3d,0x4a,0xd0,
					0x1a,0x0b,0xe5,0xa9,0x80,0xf5,0x9c,0x19,
					0xdc,0x3b,0x33,0x88,0xb5,0x44,0x2a,0x36
			}
		};

		static const SceNpTitleSecret EuropeanSecret = 
		{
			{
				0xd2,0xea,0xd3,0x8b,0x61,0xfb,0x77,0xad,
					0xc7,0x18,0x09,0xa8,0xb8,0x67,0x40,0x0d,
					0x84,0x1b,0x29,0xd4,0xc9,0x59,0xac,0x87,
					0x56,0x8a,0x28,0xad,0x7d,0x8c,0xaf,0x4a,
					0xeb,0x82,0x93,0xf4,0x58,0x34,0x3c,0x4c,
					0x0f,0x14,0x1d,0x57,0xa8,0x03,0x5b,0x5f,
					0x55,0x1d,0x42,0xff,0xb3,0x98,0xe8,0x1d,
					0xb0,0x00,0x57,0x18,0xe5,0x2d,0xf5,0xca,
					0xfd,0xec,0xea,0xd3,0xe4,0x96,0xf1,0xa4,
					0x80,0xe2,0xb2,0x06,0x79,0xf2,0x05,0x6d,
					0xc8,0x5f,0xb9,0x0d,0xdc,0x9f,0x7b,0x68,
					0x0b,0x40,0xf7,0x6d,0x23,0x8d,0xd0,0xc2,
					0xd7,0x46,0x74,0x8c,0xdc,0xe5,0x77,0x85,
					0x6e,0x7d,0x7c,0xfd,0x53,0x50,0x17,0x99,
					0x4b,0xfb,0x48,0x44,0xce,0xbd,0x7c,0xff,
					0xce,0xff,0x3e,0x8f,0x0d,0x4f,0x92,0x7b,
			}
		};

		static const SceNpTitleSecret AmericanSecret = 
		{
			{
				0xe1,0x59,0xf5,0x96,0x57,0xa6,0x16,0xcc,
					0x1d,0xe5,0x84,0x12,0xa8,0xa0,0xd8,0xa5,
					0x4b,0x14,0x30,0xc1,0x26,0x78,0xe1,0xdd,
					0xd2,0xc8,0x6c,0xbe,0x59,0xc0,0x26,0x98,
					0x09,0x49,0x2f,0x91,0x18,0xd6,0xc5,0xf0,
					0xbb,0x98,0x20,0x6d,0x77,0xe7,0xd2,0x17,
					0xad,0xcf,0xa5,0xb0,0x7f,0xa2,0x0f,0xc3,
					0x37,0x63,0xcd,0xb5,0x65,0xd3,0xdd,0xd5,
					0x79,0xfe,0x2e,0xed,0x27,0x21,0x38,0xdf,
					0x9a,0xb6,0x3c,0x9c,0x5b,0xc1,0x34,0xec,
					0xd7,0xa3,0x0e,0xb8,0x79,0xc2,0x37,0xca,
					0xed,0x60,0xe1,0xce,0xd0,0x06,0x7a,0xdd,
					0x7d,0x9d,0xda,0xfc,0x50,0x2d,0x4c,0xe8,
					0x7f,0xcb,0x24,0xa0,0x2e,0xf9,0x98,0xe0,
					0x35,0xbf,0x89,0x38,0x4e,0xd5,0xd0,0x63,
					0x46,0x74,0xee,0x6a,0x50,0x99,0xe8,0xf7,
			}
		};

		static const SceNpTitleSecret OtherSecret =
		{
			{
				0x9c,0xfe,0xcb,0x67,0xdb,0x64,0xf8,0xcd,
					0x5d,0x51,0x9f,0xe1,0xc9,0x5e,0xe9,0xb0,
					0x97,0xb4,0xd9,0xb2,0x42,0xbd,0x00,0x60,
					0xb4,0x39,0xb8,0xf7,0xca,0x3a,0x1b,0x44,
					0x56,0x61,0x6b,0xd0,0xb2,0x20,0xd8,0xd7,
					0x66,0x38,0xb0,0x03,0x6d,0x60,0x78,0xcb,
					0x34,0x99,0x83,0xe2,0x97,0x88,0x69,0x55,
					0xbb,0x4a,0x81,0xe7,0xd1,0xca,0xa4,0x9d,
					0x2d,0x57,0xf4,0x0b,0x6f,0x05,0x81,0x77,
					0x6a,0x6b,0x46,0x3e,0x2f,0x94,0xe5,0x9c,
					0xb7,0x94,0x6b,0x36,0x02,0x82,0x26,0x47,
					0x57,0xec,0x96,0x83,0xf4,0x9d,0x85,0x4e,
					0x38,0xaf,0xca,0x64,0x96,0xc5,0x52,0x23,
					0x15,0xbc,0x66,0xe6,0x87,0xf2,0x42,0x1a,
					0x4b,0x3f,0x39,0xb6,0x6b,0x23,0xb2,0xe8,
					0xf8,0xdc,0x4a,0xee,0xe9,0xbc,0xc3,0x5f,
			}
		};

		SceNpTitleId sceNpTitleId;
		SceNpTitleSecret sceNpTitleSecret;
		sysMemSet(&sceNpTitleId, 0, sizeof(sceNpTitleId));
		sysMemSet(&sceNpTitleSecret, 0, sizeof(sceNpTitleSecret));

		if (sysAppContent::IsEuropeanBuild())
		{
			safecpy(sceNpTitleId.id, "CUSA00411_00");
			sceNpTitleSecret = EuropeanSecret;
		}
		else if (sysAppContent::IsAmericanBuild())
		{
			safecpy(sceNpTitleId.id, "CUSA00419_00");
			sceNpTitleSecret = AmericanSecret;
		}
		else if (sysAppContent::IsJapaneseBuild())
		{
			safecpy(sceNpTitleId.id, "CUSA00880_00");
			sceNpTitleSecret = OtherSecret;
		}
		else
		{
			// Default to European
			safecpy(sceNpTitleId.id, "CUSA00411_00");
			sceNpTitleSecret = EuropeanSecret;
		}

		rlNpTitleId npTitleId(&npCommId, &npCommSig, &npCommPassphrase, &sceNpTitleId, &sceNpTitleSecret);

		const char rosTitleSecret[] = "C6i91R73oCD3qt1kUh0UIkDTu3Su5Qa7/r74q5ohUj1UxX/yQz7qB8a4y2TXfCMxqJo31tOPuZJMwG3jupDl7rs=";

		// Public RSA key assigned by RAGE Team.
		static u8 publicRsaKey[140] = {
			0x30, 0x81, 0x89,       //SEQUENCE (0x89 bytes = 137 bytes)
			0x02, 0x81, 0x81,   //INTEGER (0x81 bytes = 129 bytes)
			0x00,           //Leading zero of unsigned 1024 bit modulus
			0xb8, 0x93, 0x2e, 0x55, 0xb8, 0x77, 0x72, 0x78, 0x3a, 0x83, 0xa8, 0x33, 0x92, 0xe0, 0x05, 0xba, 
			0x72, 0x9a, 0xe0, 0xb6, 0x47, 0x71, 0x6f, 0x5f, 0x3d, 0x1a, 0xf8, 0x62, 0x21, 0x28, 0xda, 0xfe, 
			0xb6, 0x14, 0x93, 0xc6, 0x5d, 0x52, 0x42, 0xd1, 0x09, 0x97, 0x41, 0xf2, 0xfa, 0xae, 0x92, 0x10, 
			0xfb, 0xe9, 0x0c, 0xcc, 0x73, 0xd3, 0xcd, 0x67, 0x65, 0x47, 0xb8, 0x54, 0x92, 0xe2, 0xf0, 0x45, 
			0x35, 0xbb, 0x16, 0x05, 0xc0, 0x0d, 0xbf, 0xcd, 0x73, 0x10, 0xdb, 0xe7, 0x73, 0x0d, 0xa9, 0xa7, 
			0xe6, 0x06, 0xe8, 0xf5, 0x67, 0x12, 0x6d, 0x5f, 0x7a, 0xc2, 0x4d, 0x9a, 0xdc, 0xef, 0x2e, 0xf2, 
			0xf5, 0xb1, 0x66, 0x35, 0x9e, 0xae, 0xdc, 0x89, 0x7a, 0xda, 0x66, 0x08, 0x1f, 0xf1, 0xcd, 0x59, 
			0x29, 0x3d, 0x67, 0x74, 0xa6, 0x58, 0xf9, 0xcf, 0xf9, 0x49, 0x79, 0xd2, 0x71, 0xc1, 0xb1, 0x9d,
			0x02, 0x03,         //INTEGER (0x03 bytes = 3 bytes)
			0x01, 0x00, 0x01 //Public exponent, 65537
		};

		//  Each time the API of a deployed ROS service changes, 
		//   the version number for affected titles will be incremented, 
		//   and the game client will need to be updated to point to the new version.
		const rlRosTitleId rosTitleId(CGameConfig::Get().GetConfigOnlineServices().m_RosTitleName
			,CGameConfig::Get().GetConfigOnlineServices().m_RosScVersion
			,CGameConfig::Get().GetConfigOnlineServices().m_RosTitleVersion
			,rosTitleSecret
			,publicRsaKey, NELEM(publicRsaKey));

		rlTitleId titleId(npTitleId, rosTitleId);

		g_rlNp.SetDefaultSessionImage("platform:/data/icon.jpg");

#elif RSG_DURANGO
		const char rosTitleSecret[] = "CzNMuF1g9qTpMaAkczKPCFNicASIHUuvUw8932yltFRG3jfTB/t5OKaI3rDiuwVH35yMDfUmykmxxzEzXIDg44g=";

		// Public RSA key assigned by RAGE Team.
		static u8 publicRsaKey[140] = {
			0x30, 0x81, 0x89,       //SEQUENCE (0x89 bytes = 137 bytes)
			0x02, 0x81, 0x81,   //INTEGER (0x81 bytes = 129 bytes)
			0x00,           //Leading zero of unsigned 1024 bit modulus
			0xb1, 0x59, 0x94, 0xaf, 0x66, 0xd4, 0xd8, 0x14, 0xb3, 0x5f, 0x31, 0x04, 0xc8, 0xa9, 0x11, 0x1a, 
			0x81, 0x78, 0xb3, 0x57, 0x2e, 0x92, 0xd5, 0xfc, 0x80, 0x3e, 0xda, 0x7d, 0x99, 0x35, 0x54, 0x97, 
			0xf8, 0xf8, 0x63, 0x40, 0x7f, 0xe5, 0x1d, 0x4e, 0xf0, 0x92, 0x00, 0xe5, 0x66, 0x58, 0x0d, 0x65, 
			0xd9, 0x03, 0xa4, 0xa7, 0x36, 0xd9, 0x87, 0x09, 0x14, 0xf1, 0x06, 0x86, 0x3d, 0x3c, 0x4c, 0x15,
			0xde, 0xdb, 0x8c, 0x28, 0x70, 0xe2, 0x3c, 0x66, 0xcc, 0xf4, 0xe8, 0xa5, 0x81, 0x4a, 0x0d, 0x74, 
			0x2b, 0x5d, 0xd1, 0x0d, 0xa6, 0xb2, 0xf1, 0xb6, 0x2c, 0x63, 0xab, 0x35, 0x48, 0xd6, 0x03, 0x69, 
			0xf9, 0x83, 0x27, 0xa0, 0x49, 0x62, 0x86, 0xeb, 0xd1, 0x00, 0x7d, 0xbf, 0x4d, 0x6f, 0xfa, 0x58, 
			0x60, 0x70, 0xe0, 0x93, 0x72, 0x22, 0x0d, 0xc5, 0x3a, 0x50, 0x36, 0xef, 0x7a, 0x54, 0x31, 0xcd,
			0x02, 0x03,         //INTEGER (0x03 bytes = 3 bytes)
			0x01, 0x00, 0x01 //Public exponent, 65537
		};

		//  Each time the API of a deployed ROS service changes, 
		//   the version number for affected titles will be incremented, 
		//   and the game client will need to be updated to point to the new version.
		const rlRosTitleId rosTitleId(
			CGameConfig::Get().GetConfigOnlineServices().m_RosTitleName,
			CGameConfig::Get().GetConfigOnlineServices().m_RosScVersion,
			CGameConfig::Get().GetConfigOnlineServices().m_RosTitleVersion,
			rosTitleSecret,
			publicRsaKey, 
			NELEM(publicRsaKey));

		const char* sessionTemplate = nullptr;
		if(!PARAM_netXblSessionTemplateName.Get(sessionTemplate))
			sessionTemplate = CGameConfig::Get().GetConfigOnlineServices().m_MultiplayerSessionTemplateName;

		rlXblTitleId xblTitleId(sessionTemplate);
		rlTitleId titleId(xblTitleId, rosTitleId);
#elif __WIN32PC

		// Rockstar Online Services Title Info, title name is assigned by RAGE Team.
		const char rosTitleSecret[] = "C4pWJwWIKGUxcHd69eGl2AOwH2zrmzZAoQeHfQFcMelybd32QFw9s10px6k0o75XZeB5YsI9Q9TdeuRgdbvKsxc=";

		//  Each time the API of a deployed ROS service changes, 
		//   the version number for affected titles will be incremented, 
		//   and the game client will need to be updated to point to the new version.

		rlRosEnvironment rosEnv = RL_DEFAULT_ROS_ENVIRONMENT;

		// Public RSA key assigned by RAGE Team.
		static u8 publicRsaKey[140] = {
			0x30, 0x81, 0x89,       //SEQUENCE (0x89 bytes = 137 bytes)
			0x02, 0x81, 0x81,   //INTEGER (0x81 bytes = 129 bytes)
			0x00,           //Leading zero of unsigned 1024 bit modulus
			0xab, 0xc1, 0x95, 0x54, 0x2b, 0x9c, 0x48, 0x52, 0x36, 0x71, 0xc9, 0x15, 0x2b, 0x09, 0xb8, 0x59, 
			0xd0, 0x16, 0xd5, 0xa4, 0x2a, 0x7a, 0x84, 0xfc, 0x55, 0xf6, 0x5d, 0x79, 0x2b, 0xcd, 0x81, 0x78, 
			0xa3, 0x2b, 0x02, 0x7d, 0x7f, 0xfc, 0x34, 0xee, 0x4f, 0x18, 0x73, 0xf5, 0xde, 0xc1, 0x22, 0xc7, 
			0xfc, 0xc4, 0x2b, 0xfe, 0xaa, 0x8d, 0xc8, 0x05, 0xcc, 0x40, 0x97, 0xcf, 0xea, 0x0a, 0x5a, 0x42, 
			0xb0, 0x24, 0xb7, 0xe6, 0x17, 0x6c, 0x9f, 0x1c, 0xbe, 0x17, 0xa7, 0x51, 0xb8, 0xf5, 0xda, 0x9b, 
			0xef, 0x25, 0x1a, 0xe0, 0xe1, 0x1b, 0x8e, 0x80, 0x12, 0x5b, 0x52, 0x3e, 0x49, 0x5b, 0xd5, 0xf5, 
			0xbb, 0x5b, 0x0e, 0xb0, 0x6c, 0x7d, 0x35, 0x02, 0x22, 0x32, 0xc9, 0xcf, 0x80, 0xa4, 0x94, 0x4c, 
			0x12, 0x26, 0x40, 0x0b, 0xda, 0x81, 0xdd, 0x6e, 0x65, 0xd9, 0x3d, 0xc4, 0x44, 0x6b, 0x42, 0x17,
			0x02, 0x03,         //INTEGER (0x03 bytes = 3 bytes)
			0x01, 0x00, 0x01 //Public exponent, 65537
		};

		const rlRosTitleId rosTitleId(CGameConfig::Get().GetConfigOnlineServices().m_RosTitleName
			,CGameConfig::Get().GetConfigOnlineServices().m_TitleDirectoryName
			,CGameConfig::Get().GetConfigOnlineServices().m_RosScVersion
			,CGameConfig::Get().GetConfigOnlineServices().m_RosTitleVersion
			,rosTitleSecret
#if __STEAM_BUILD
			,g_rlPc.GetSteamAuthTicket()
			,CGameConfig::Get().GetConfigOnlineServices().m_SteamAppId
#endif
			,rosEnv
			,publicRsaKey, NELEM(publicRsaKey));

		rlTitleId titleId(rosTitleId);

#endif  // RSG_ORBIS

		//Don't quit on failure to allocate Rline memory.
		//Should trigger OUT OF MEMORY event and exit network.
		sm_RlineAllocator->SetQuitOnFail(false);

		unsigned MIN_AGE_RATING;
		if (sysAppContent::IsEuropeanBuild())
		{
			MIN_AGE_RATING = 18;
			gnetDebug1("European Build - Minimum Age Rating is %d", MIN_AGE_RATING);
		}
		else if (sysAppContent::IsAmericanBuild())
		{
			MIN_AGE_RATING = 17;
			gnetDebug1("American Build - Minimum Age Rating is %d", MIN_AGE_RATING);
		}
		else if (sysAppContent::IsJapaneseBuild())
		{
			MIN_AGE_RATING = 18;
			gnetDebug1("Japanese Build - Minimum Age Rating is %d", MIN_AGE_RATING);
		}
		else
		{
			MIN_AGE_RATING = 18;
#if defined(__GERMAN_BUILD) && __GERMAN_BUILD
			gnetDebug1("German Build - Minimum Age Rating is %d", MIN_AGE_RATING);
#elif defined(__AUSSIE_BUILD) && __AUSSIE_BUILD
			gnetDebug1("Aussie Build - Minimum Age Rating is %d", MIN_AGE_RATING);
#else
			gnetDebug1("Invalid Build - Minimum Age Rating is %d", MIN_AGE_RATING);
#endif
		}

		// Language needs to be set before rlInit
#if RSG_PC
		// -uilanguage can override the system language
		int language = LANGUAGE_UNDEFINED;

		const char* langString;
		if(PARAM_uilanguage.Get(langString))
		{
			language = TheText.GetLanguageFromName(langString);
		}

		if(language == LANGUAGE_UNDEFINED)
		{
			language = CPauseMenu::GetLanguageFromSystemLanguage();
		}

		// Let rlPc and the RGSC DLL know of additional presence attributes to use for determining
		// if the RGSC UI should present a "Join Game" option
		g_rlPc.SetAdditionalSessionAttr(ATTR_TRANSITION_INFO);
		g_rlPc.SetAdditionalJoinAttr(ATTR_TRANSITION_IS_JOINABLE);

		char buf[rgsc::RGSC_MAX_PATH] = {0};
#if __FINAL
		if (ASSET.Exists("update/x64/metadata.dat", NULL))
		{
			ASSET.FullReadPath(buf, sizeof(buf), "update/x64/metadata.dat", NULL);
			g_rlPc.SetMetaDataPath(buf);
		}
#else
		const fiDevice *device = fiDevice::GetDevice("update:/x64/metadata.dat", true);
		if (device && ASSET.Exists("update:/x64/metadata.dat", NULL))
		{
			device->FixRelativeName(buf, sizeof(buf), "update:/x64/metadata.dat");
			g_rlPc.SetMetaDataPath(buf);
		}
#endif

		rlSetLanguage((rage::sysLanguage)language);
#else
		sysLanguage language = g_SysService.GetSystemLanguage();
		if (language != LANGUAGE_UNDEFINED)
		{
			rlSetLanguage(language);
		}
		else
		{
			gnetError("System Language is undefined, running a build not supporting the system language?");
		}
#endif
		
		rlInit(sm_RlineAllocator, sm_SocketMgr->GetMainSocket(), &titleId, MIN_AGE_RATING);

#if __WIN32PC
		// PC Downloader Pipe
		g_rlPc.ConnectDownloaderPipe("\\\\.\\pipe\\GTAVLauncher_Pipe");
#endif

		// add callbacks before initialising presence and ros so that we receive Init() events
		rlPresence::AddDelegate(&sm_PresenceDlgt);
		rlRos::AddDelegate(&sm_RosDlgt);
		rlFriendsManager::AddDelegate(&sm_FriendDlgt);
		MEMBERSHIP_ONLY(rlScMembership::AddDelegate(&sm_MembershipDlgt));
		g_SysService.AddDelegate(&sm_ServiceDelegate);
#if ENABLE_SINGLE_PLAYER_CLOUD_SAVES
		g_rlPc.AddDelegate(&sm_PcDlgt);
#endif

		//@@: } CLIVEMANAGER_INIT_INNER
		// refresh privileges
		RefreshPrivileges();

		sm_AchMgr.Init();
		sm_InviteMgr.Init();
		sm_PartyInviteMgr.Init();
		sm_FriendsPage.Init(FRIEND_CACHE_PAGE_SIZE);
		sm_FriendsReadBuf.Init(FRIEND_CACHE_PAGE_SIZE);

#if RSG_DURANGO
		events_durango::Init();
#endif

		//@@: location CLIVEMANAGER_INIT_INSTANTIATE_CLOUD_MANAGER
		CloudManager::Instantiate();

#if defined(GTA_REPLAY) && GTA_REPLAY	
		VideoUploadManager::Instantiate();
#endif
		NetworkClanInst::Instantiate();
		UserContentManager::Instantiate();
		SCPresenceSingleton::Instantiate();
		CGamePresenceEventDispatcherSingleton::Instantiate();
		CSocialClubInboxMgrSingleton::Instantiate();
		NetworkGamerAttributesSingleton::Instantiate();
		SPolicyVersions::Instantiate();
		NetworkLegalRestrictionsInst::Instantiate();
		NET_SHOP_ONLY(GameTransactionSessionMgr::Get().Init());
		NET_SHOP_ONLY(NetworkShoppingMgrSingleton::Instantiate();)
		NET_SHOP_ONLY(CatalogServerDataInst::Instantiate();)

		//Be sure to initialize the friends reader before
		//the presence manager so we can handle friends-related
		//presence events from the get-go.
		success = gnetVerify(g_SystemUi.Init());

		sm_FriendsInMatch = 0;

		rlPresence::ClearActingGamerInfo();

#if RSG_XBOX
		sm_SignedOutHandle.Clear();
		sm_SignedInIntoNewProfileAfterSuspendMode = m_SignedInIntoNewProfileAfterSuspendMode_Default;
#endif

		sm_IsInitialized = true;

		if(!success)
		{
			CLiveManager::Shutdown(SHUTDOWN_CORE);
		}

		netSCGamerConfigParser::Init("common:/data/playerxlast.xml");

		NETWORK_CLAN_INST.Init(initMode);
		sm_NetworkClanDataMgr.Init();

		GetSocialClubMgr().Init();
#if RL_FACEBOOK_ENABLED
		sm_Facebook.Init();
#endif // #if RL_FACEBOOK_ENABLED
		sm_Commerce.Init();
		CNetworkNewsStoryMgr::Get().Init();
		CNetworkEmailMgr::Get().Init();
#if GEN9_STANDALONE_ENABLED
        CSocialClubFeedMgr::Get().Init();
        CSocialClubFeedTilesMgr::Get().Init();
#endif

		rlScAuth::SetTitleId(CGameConfig::Get().GetConfigOnlineServices().m_ScAuthTitleId);
		sm_scGamerDataMgr.Init(CGameConfig::Get().GetConfigOnlineServices().m_RosTitleName);
		SocialClubEventMgr::Get().Init(CGameConfig::Get().GetConfigOnlineServices().m_RosTitleName);

		sm_AddFriendStatus.Reset();

		CloudManager::GetInstance().Init();
		UserContentManager::GetInstance().Init();

#if defined(GTA_REPLAY) && GTA_REPLAY	
		VideoUploadManager::GetInstance().Init();
#endif

		PRESENCE_EVENT_DISPATCH.Init();
		GamePresenceEvents::RegisterEvents();

		SC_INBOX_MGR.Init();

		NETWORK_GAMERS_ATTRS.Init();

		NET_SHOP_ONLY(NETWORK_SHOPPING_MGR.Init(CLiveManager::GetRlineAllocator());)

			SCPlayerCardCoronaManager::Instantiate();

#if ENABLE_SINGLE_PLAYER_CLOUD_SAVES
		// single player cloud saves
		if (g_rlPc.IsAtLeastSocialClubVersion(1,1,7,3))
		{
			InitSinglePlayerCloudSaves();
		}
#else
		gnetDebug3("Single player cloud saves not enabled.");
#endif

#if RSG_XDK
		// we have our own alert / dialog, don't need this
		// additionally, this will show when we don't want it to
		g_rlXbl.GetPartyManager()->SuppressGameSessionReadyNotification(true);
#endif
	}
	else if (initMode == INIT_SESSION && sm_IsInitialized)
	{
#if RSG_XBOX
		sm_SignedInIntoNewProfileAfterSuspendMode = m_SignedInIntoNewProfileAfterSuspendMode_Default;
#endif

		if(NetworkInterface::IsLocalPlayerOnline())
			sm_ProfileStatsMgr.Init();

		UserContentManager::GetInstance().ResetContentHash();

		NETWORK_CLAN_INST.Init(initMode);

		NET_SHOP_ONLY(NETWORK_SHOPPING_MGR.InitCatalog();)

        sm_bInitialisedSession = true; 
        CheckAndSendSubscriptionTelemetry();

#if GEN9_STANDALONE_ENABLED
        // We stop caching UI images when the game starts up
        CSocialClubFeedTilesMgr::Get().SetCacheImagesHint(false);
#endif
	}

	NOTFINAL_ONLY(CheckDEVAccount();)

		return success;
	//@@: } CLIVEMANAGER_INIT
}

void CLiveManager::Shutdown(const unsigned shutdownMode)
{
	gnetDebug1("Shutdown");

	sm_AchMgr.Shutdown(shutdownMode);
	sm_RichPresenceMgr.Shutdown(shutdownMode);

	if(SCPlayerCardCoronaManager::IsInstantiated())
		SCPlayerCardCoronaManager::GetInstance().Shutdown(shutdownMode);

	if(shutdownMode == SHUTDOWN_CORE)
	{
		if(sm_IsInitialized)
		{
			NET_SHOP_ONLY(NETWORK_SHOPPING_MGR.Shutdown( shutdownMode );)
			NET_SHOP_ONLY(GameTransactionSessionMgr::Get().Shutdown());

			NETWORK_GAMERS_ATTRS.Shutdown();
			NetworkGamerAttributesSingleton::Destroy();

			rlPresence::RemoveDelegate(&sm_PresenceDlgt);
			rlRos::RemoveDelegate(&sm_RosDlgt);
			rlFriendsManager::RemoveDelegate(&sm_FriendDlgt);
			MEMBERSHIP_ONLY(rlScMembership::RemoveDelegate(&sm_MembershipDlgt));
			g_SysService.RemoveDelegate(&sm_ServiceDelegate);
#if ENABLE_SINGLE_PLAYER_CLOUD_SAVES
			g_rlPc.RemoveDelegate(&sm_PcDlgt);
#endif

			NETWORK_LEGAL_RESTRICTIONS.Shutdown();
			SC_INBOX_MGR.Shutdown();
			PRESENCE_EVENT_DISPATCH.Shutdown();
			sm_FriendsInMatch = 0;
			g_SystemUi.Shutdown();
			sm_InviteMgr.Shutdown();
			sm_PartyInviteMgr.Shutdown();
			sm_FriendsPage.Shutdown();
			sm_FriendsReadBuf.Shutdown();
			GetSocialClubMgr().Shutdown();
#if RL_FACEBOOK_ENABLED
			sm_Facebook.Shutdown();
#endif // #if RL_FACEBOOK_ENABLED
			sm_scGamerDataMgr.Shutdown();
            sm_Commerce.Shutdown();
			SocialClubEventMgr::Get().Shutdown();

			rlPresence::ClearActingGamerInfo();

#if RSG_XBOX
			sm_SignedOutHandle.Clear();
			sm_SignedInIntoNewProfileAfterSuspendMode = m_SignedInIntoNewProfileAfterSuspendMode_Default;
#endif

			CNetworkNewsStoryMgr::Get().Shutdown();
			CNetworkEmailMgr::Get().Shutdown();
#if GEN9_STANDALONE_ENABLED
            CSocialClubFeedMgr::Get().Shutdown();
            CSocialClubFeedTilesMgr::Get().Shutdown();
#endif

			rlShutdown();
			netShutdown();

			sm_IsInitialized = false;

			netSCGamerConfigParserSingleton::Destroy();

			NETWORK_CLAN_INST.Shutdown(shutdownMode);

			CloudManager::GetInstance().Shutdown();
            CloudManager::Destroy();

#if defined(GTA_REPLAY) && GTA_REPLAY	
			VideoUploadManager::GetInstance().Shutdown();
			VideoUploadManager::Destroy();
#endif

			UserContentManager::GetInstance().Shutdown();
            UserContentManager::Destroy();

			SCPresenceSingleton::Destroy();
			SPolicyVersions::Destroy();

            CNetwork::UseNetworkHeap(NET_MEM_RLINE_HEAP);

			if(sm_SocketMgr)
			{
				sm_SocketMgr->Shutdown();
				delete sm_SocketMgr;
				sm_SocketMgr = nullptr;
			}
			
            delete [] s_RlineHeap;
            delete sm_RlineAllocator;
            s_RlineHeap = 0;
            sm_RlineAllocator = 0;
            CNetwork::StopUsingNetworkHeap();

            CNetwork::DeleteNetworkHeap();

			NetworkDebug::Shutdown();
			SCPlayerCardCoronaManager::Destroy();
		}

		BANK_ONLY(CLiveManager::WidgetShutdown();)
	}
    else if(shutdownMode == SHUTDOWN_SESSION)
    {
        sm_bInitialisedSession = false;
    }

	sm_ProfileStatsMgr.Shutdown(shutdownMode);
	sm_NetworkClanDataMgr.Shutdown(shutdownMode);
}

netSocketManager*
CLiveManager::GetSocketMgr()
{
	gnetAssert(sm_SocketMgr != nullptr);
	return sm_SocketMgr;
}

bool CLiveManager::ShouldBailFromMultiplayer()
{
    return CNetwork::IsNetworkOpen() || NetworkInterface::IsAnySessionActive();
}

void CLiveManager::BailOnMultiplayer(const eBailReason nBailReason, const int nBailContext)
{
    if(!ShouldBailFromMultiplayer())
        return;

    gnetDebug1("BailOnMultiplayer :: Bailing! PauseMenu: %s, Version: %08x", CPauseMenu::IsActive(PM_SkipStore) ? "Active" : "Inactive", CPauseMenu::GetCurrentMenuVersion());

    // the store menu handles closing itself, script handle character select / creator
    if(CPauseMenu::IsActive(PM_SkipStore) && !((CPauseMenu::GetCurrentMenuVersion() == FE_MENU_VERSION_MP_CHARACTER_CREATION) || (CPauseMenu::GetCurrentMenuVersion() == FE_MENU_VERSION_MP_CHARACTER_SELECT))) 
		CPauseMenu::Close();

	CFocusEntityMgr::GetMgr().SetDefault();			// clear this here, if being forced out of MP

	INSTANCE_STORE.RemoveUnrequired(true);
	gRenderThreadInterface.Flush();											// allow protected archetypes to time out
	g_MapTypesStore.RemoveUnrequired();										// remove unwanted streamed archetype files

#if __BANK
	Displayf("****************************************");
	Displayf("** MP Bail - dump streaming interests **");

	g_strStreamingInterface->DumpStreamingInterests();
	CStreamingDebug::DisplayObjectsInMemory(NULL, false, g_MapDataStore.GetStreamingModuleId(),(1 << STRINFO_LOADED));

	Displayf("****************************************");
#endif //__BANK

    // bail out of the network game.
    CNetwork::Bail(sBailParameters(nBailReason, nBailContext));
}

bool CLiveManager::ShouldBailFromVoiceSession()
{
    return CNetwork::GetNetworkVoiceSession().IsVoiceSessionActive();
}

void CLiveManager::BailOnVoiceSession()
{
    if(!ShouldBailFromVoiceSession())
        return;

    gnetDebug1("BailOnVoiceSession :: Voice Session is Active. Bailing!");

    // handle voice session here - mirror how the main session is bailed
    CNetwork::GetNetworkVoiceSession().Shutdown(true);
    CNetwork::GetNetworkVoiceSession().Init(CLiveManager::GetRlineAllocator(), &CNetwork::GetConnectionManager(), NETWORK_SESSION_VOICE_CHANNEL_ID);
}

void CLiveManager::HandleInviteToDifferentUser(int nUser)
{
	// set the new controller index
	CControlMgr::SetMainPlayerIndex(nUser);

	// bail from any multiplayer activity
	if(ShouldBailFromMultiplayer())
	{
		gnetDebug1("HandleInviteToDifferentUser :: Triggering network bail!");
		CLiveManager::BailOnMultiplayer(BAIL_PROFILE_CHANGE, BAIL_CTX_PROFILE_CHANGE_INVITE_DIFFERENT_USER);
	}

    // bail from any active phone call
    if(ShouldBailFromVoiceSession())
    {
        gnetDebug1("HandleInviteToDifferentUser :: Triggering voice session bail!");
        CLiveManager::BailOnVoiceSession();
    }

	// restart the game
	gnetDebug1("HandleInviteToDifferentUser :: Start new game!");
#if RSG_PC
	CGame::ReInitStateMachine(CGameLogic::GetCurrentLevelIndex());
#else
	CGame::StartNewGame(CGameLogic::GetCurrentLevelIndex());
#endif

	StatsInterface::SignedOut();

#if RSG_XBOX
	sm_PostponeProfileChange = false;
#endif
}

void CLiveManager::CheckForSignOut()
{
#if RSG_XBOX
	static int s_signedOutCounter = 0;

	const bool signedOut = !NetworkInterface::IsLocalPlayerSignedIn() || CLiveManager::SignedInIntoNewProfileAfterSuspendMode();

	// Fail safe check to prevent having a user on the landing page
	if (signedOut && (++s_signedOutCounter) >= 3 && CanActionProfileChange())
	{
		uiWarningf("Unmanaged sign out while on the landing page. Falling back to the IIS");
		CLiveManager::ActionProfileChange();
		s_signedOutCounter = 0;
	}
	else if (!signedOut)
	{
		s_signedOutCounter = 0;
	}
#endif
}

#if ENABLE_SINGLE_PLAYER_CLOUD_SAVES
void CLiveManager::InitSinglePlayerCloudSaves()
{
	class GTA5CloudSaveTitleConfiguration : public rgsc::ICloudSaveTitleConfigurationV1
	{
	public:

		virtual ~GTA5CloudSaveTitleConfiguration(){}

		// PURPOSE
		//	Returns the title name for the configuration
		virtual const char* RGSC_CALL GetTitleName()
		{
			return "gta5";
		}

		// PURPOSE
		//	Returns the number of cloud save files in the title configuration
		virtual u32 RGSC_CALL GetNumCloudFiles()
		{
			return 16;
		}

		// PURPOSE
		//	Returns the file name of the configured file at a given index
		virtual const char* RGSC_CALL GetFileName(unsigned index)
		{
			switch (index)
			{
			case  0: return "SGTA50000";
			case  1: return "SGTA50001";
			case  2: return "SGTA50002";
			case  3: return "SGTA50003";
			case  4: return "SGTA50004";
			case  5: return "SGTA50005";
			case  6: return "SGTA50006";
			case  7: return "SGTA50007";
			case  8: return "SGTA50008";
			case  9: return "SGTA50009";
			case 10: return "SGTA50010";
			case 11: return "SGTA50011";
			case 12: return "SGTA50012";
			case 13: return "SGTA50013";
			case 14: return "SGTA50014";
			case 15: return "SGTA50015";
			default: return NULL;
			}
		}

		// PURPOSE
		//	Returns the directory that houses the per-profile/per-title save data
		virtual const char* RGSC_CALL GetSaveDirectory()
		{
			static char buffer[rgsc::RGSC_MAX_PATH];
			g_rlPc.GetFileSystem()->GetTitleProfileDirectory(buffer, true);
			return buffer;
		}

		rgsc::RGSC_HRESULT RGSC_CALL QueryInterface(rgsc::RGSC_REFIID riid, void** ppvObj)
		{
			if(ppvObj == NULL)
			{
				return rgsc::RGSC_INVALIDARG;
			}

			if(riid == rgsc::IID_IRgscUnknown)
			{
				*ppvObj = static_cast<rgsc::ICloudSaveTitleConfiguration*>(this);
			}
			else if(riid == rgsc::IID_ICloudSaveTitleConfigurationV1)
			{
				*ppvObj = static_cast<rgsc::ICloudSaveTitleConfigurationV1*>(this);
			}
			else
			{
				*ppvObj = NULL;
				return rgsc::RGSC_NOINTERFACE;
			}

			return rgsc::RGSC_OK;
		}

		virtual const char* RGSC_CALL GetHardwareId()
		{
			// not used by game, only used by launcher when posting/syncing cloud files
			return "";
		}
	};

	static GTA5CloudSaveTitleConfiguration s_CloudSaveConfiguration;

	rtry
	{
		rverify(g_rlPc.GetCloudSaveManager(), catchall, );
		m_CloudSaveManifest = g_rlPc.GetCloudSaveManager()->RegisterTitle(&s_CloudSaveConfiguration);
		rverify(m_CloudSaveManifest, catchall, );

		HRESULT hr = m_CloudSaveManifest->QueryInterface(rgsc::IID_ICloudSaveManifestV2, (void**)&m_CloudSaveManifestV2);
		rverify(SUCCEEDED(hr) && m_CloudSaveManifestV2, catchall, );

		gnetDebug3("Single player cloud saves initialized.");
	}
	rcatchall
	{
		gnetError("Single player cloud saves failed to initialized.");
	}
}

bool CLiveManager::RegisterSinglePlayerCloudSaveFile(const char* filename, const char* metadata)
{
	rtry
	{
		rverify(filename && filename[0] != '\0', catchall, );
		rverify(metadata && metadata[0] != '\0', catchall, );
		rverify(m_CloudSaveManifestV2, catchall, );
		rverify(g_rlPc.GetCloudSaveManager(), catchall, );
		rverify(g_rlPc.GetCloudSaveManager()->RegisterFileForUpload(m_CloudSaveManifestV2, filename, metadata), catchall, );
		return true;
	}
	rcatchall
	{
		return false;
	}
}

bool CLiveManager::UnregisterSinglePlayerCloudSaveFile(const char* filename)
{
	rtry
	{
		rverify(filename && filename[0] != '\0', catchall, );
		rverify(m_CloudSaveManifestV2, catchall, );
		rverify(g_rlPc.GetCloudSaveManager(), catchall, );
		rverify(g_rlPc.IsAtLeastSocialClubVersion(1,1,7,5), catchall, );
		rverify(g_rlPc.GetCloudSaveManager()->UnregisterFile(m_CloudSaveManifestV2, filename), catchall, );
		return true;
	}
	rcatchall
	{
		return false;
	}
}

bool CLiveManager::UpdatedSinglePlayerCloudSaveEnabled(const char* rosTitleName)
{
	rtry
	{
		rverify(m_CloudSaveManifestV2, catchall, );
		rcheck(stricmp(m_CloudSaveManifestV2->GetTitleName(), rosTitleName) == 0, catchall, );

		// cache the cloud saves enabled state
		rgsc::ICloudSaveManifestV2::CloudSavesEnabledState state = m_CloudSaveManifestV2->GetCloudSaveEnabled();

		// load the manifest from disk to ensure we have the most up to date data. 
		// If the manifest already exists on disk, the most recent on-disk version will be loaded.
		//	We'll then update using the cached value.
		rverify(m_CloudSaveManifestV2->Load(), catchall, );

		// update the enabled state from cache
		m_CloudSaveManifestV2->SetCloudSavesEnabled(state);

		// save to disk
		rverify(m_CloudSaveManifestV2->Save(), catchall, );

		return true;
	}
	rcatchall
	{
		return false;
	}
}
#endif // RSG_PC

#if COMMERCE_CONTAINER
void CLiveManager::CommerceUpdate()
{
	const unsigned curTime = sysTimer::GetSystemMsTime() | 0x01;

	if (sm_Commerce.ContainerIsStoreMode())
	{
		sm_LastUpdateTime = curTime;
		if(sm_SocketMgr)
		{
			sm_SocketMgr->Update();
		}

		netUpdate();
		rlUpdate();

		sm_Commerce.Update();
		CloudManager::GetInstance().Update();
	}
	else
	{		
		Update(curTime);
	}
}
#endif

PARAM(netVerboseCanActionProfileChange, "[live] Verbose logging of whether or not we can action a profile change");

int CLiveManager::GetProfileChangeActionDelayMs()
{
	static const int DEFAULT_COUNTER_TO_ACTION_PROFILE_CHANGE = 2500;
	if(Tunables::IsInstantiated())
		return Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NET_PROFILE_CHANGE_DELAY_MS", 0x6fd0e36c), DEFAULT_COUNTER_TO_ACTION_PROFILE_CHANGE);

	return DEFAULT_COUNTER_TO_ACTION_PROFILE_CHANGE;
}

bool CLiveManager::HasRecentResume()
{
	// if we have never resumed, return false
	if(sm_LastResumeTimestamp == 0)
		return false;

	static const int DEFAULT_RECENT_RESUME_TIME_THRESHOLD = 2500;

	int recentResumeThresholdMs = DEFAULT_RECENT_RESUME_TIME_THRESHOLD;
	if(Tunables::IsInstantiated())
		recentResumeThresholdMs = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NET_RECENT_RESUME_TIME_THRESHOLD", 0xcc9b762c), DEFAULT_RECENT_RESUME_TIME_THRESHOLD);

	return (sysTimer::GetSystemMsTime() - sm_LastResumeTimestamp) < recentResumeThresholdMs;
}

bool IsOnInitialInteractiveScreen()
{
#if RSG_GEN9
	return CInitialInteractiveScreen::IsActive();
#else
	return false;
#endif
}

bool CLiveManager::CanActionProfileChange(OUTPUT_ONLY(bool forceVerboseLogging))
{
#if !__NO_OUTPUT
	const bool verboseLogging = PARAM_netVerboseCanActionProfileChange.Get() || forceVerboseLogging;
#endif

	// PC Allows us to immediately create and log into a new account in the SCUI while the new game is being deferred,
	//	resulting in unexpected/undefined behaviour. PC also has a unique game session state machine flow due to 
	//	entitlements which begins with a tight loop where the level shuts down and requires a new login. 
#if !RSG_PC
	// The IIS check is here so we're sure a pending sign out is actioned at the latest
	// as soon as we reach that page.
	if(CGame::IsChangingSessionState() && !IsOnInitialInteractiveScreen())
	{
		OUTPUT_ONLY(if (verboseLogging) { gnetDebug3("CanActionProfileChange - FALSE - Changing session state."); })
		return false;
	}

	if(g_SystemUi.IsUiShowing())
	{
		OUTPUT_ONLY(if (verboseLogging) { gnetDebug3("CanActionProfileChange - FALSE - System UI is showing."); })
			return false;
	}

	if(CPauseMenu::IsActive())
	{
		OUTPUT_ONLY(if (verboseLogging) { gnetDebug3("CanActionProfileChange - FALSE - Pause Menu is active."); })
		return false;
	}

	if(CGameSessionStateMachine::GetGameSessionState() != CGameSessionStateMachine::IDLE && !IsOnInitialInteractiveScreen())
	{
		OUTPUT_ONLY(if (verboseLogging) { gnetDebug3("CanActionProfileChange - FALSE - GetGameSessionState() != CGameSessionStateMachine::IDLE."); })
		return false;
	}

	if(CGenericGameStorage::IsSavegameLoadInProgress())
	{
		OUTPUT_ONLY(if (verboseLogging) { gnetDebug3("CanActionProfileChange - FALSE - Save Load In Progress."); })
		return false;
	}

	// Landing and IIS count as loading screen
	if(CLoadingScreens::AreActive() && !NetworkInterface::IsOnLandingPage() && !IsOnInitialInteractiveScreen())
	{
		OUTPUT_ONLY(if (verboseLogging) { gnetDebug3("CanActionProfileChange - FALSE - Loading screens are active."); })
		return false;
	}

	if(CLoadingScreens::LoadingScreensPendingShutdown())
	{
		OUTPUT_ONLY(if (verboseLogging) { gnetDebug3("CanActionProfileChange - FALSE - Loading screens are pending shutdown."); })
		return false;
	}

	if(CWarningScreen::IsActive())
	{
		OUTPUT_ONLY(if (verboseLogging) { gnetDebug3("CanActionProfileChange - FALSE - Warning screen is active."); })
		return false;
	}

	if(g_PlayerSwitch.IsActive())
	{
		OUTPUT_ONLY(if (verboseLogging) { gnetDebug3("CanActionProfileChange - FALSE - Player switch is active."); })
		return false;
	}

	if(!CControlMgr::GetPlayerPad())
	{
		OUTPUT_ONLY(if (verboseLogging) { gnetDebug3("CanActionProfileChange - FALSE - Player pad does not exist."); })
		return false;
	}

	if(CControlMgr::GetPlayerPad() && !CControlMgr::GetPlayerPad()->IsConnected())
	{
		OUTPUT_ONLY(if (verboseLogging) { gnetDebug3("CanActionProfileChange - FALSE - Player pad not connected."); })
		return false;
	}
#elif !__NO_OUTPUT
	(void)verboseLogging;
#endif // !RSG_PC

	//Counter is for the the standby mode where we want to give it some time before actually starting a new game
	if((sm_PostponeProfileChange || sm_ProfileChangeActionCounter <= 0))
		return true;

	return false;
}

bool CLiveManager::HasPendingProfileChange()
{
	return sm_PostponeProfileChange
		XBOX_ONLY(|| (sm_SignedOutHandle.IsValid() && !rlPresence::IsActingGamerInfoValid()));
}

void CLiveManager::ActionProfileChange()
{
	gnetDebug1("ActionProfileChange");

#if RSG_XBOX
	sm_SignedOutHandle.Clear();
	StatsInterface::SignedOut();
#endif

	// reset this now that we have actioned the profile change
	sm_PostponeProfileChange = false;
	sm_ProfileChangeActionCounter = 0;

#if RSG_GEN9
	if(CLandingPage::IsActive())
	{
		gnetDebug1("ActionProfileChange :: Dismissing landing page");
	}

	// Ignore any script or code. If CanActionProfileChange is true let's just bail.
	// Shouldn't cause any issue since we shut down everything anyway.
	if(!CInitialInteractiveScreen::IsActive())
	{
		gnetDebug1("ActionProfileChange :: Shutting down for IIS");
		CGameSessionStateMachine::SetState(CGameSessionStateMachine::SHUTDOWN_FOR_IIS);
	}
#else
	// this actually executes the profile change - on Gen9, this is handled
	// in script when we process the event so skip this
	OnActionProfileChange();
#endif

	// let script know that a change has been actioned
	GetEventScriptNetworkGroup()->Add(CEventNetworkSignInChangeActioned());
}

void CLiveManager::OnProfileChangeCommon()
{
	camInterface::FadeOut(0);
	CNetwork::ClearTransitionTracker();

	// loading screen handling
	CLoadingScreens::OnProfileChange();
}

void CLiveManager::OnActionProfileChange()
{
	gnetDebug1("OnActionProfileChange");

	// call through to let game flow manage the profile change
	OnProfileChangeCommon();
	CGame::HandleSignInStateChange();
}

void CLiveManager::UpdatePendingProfileChange(const int timestep)
{
	if(HasPendingProfileChange())
	{
		// counter is for the the standby mode where we want to give it some time before actually starting a new game
		sm_ProfileChangeActionCounter -= timestep;

		// check if we can action the change...
		if(CanActionProfileChange())
		{
			gnetDebug1("UpdatePendingProfileChange :: Processing profile change");
			ActionProfileChange();
		}
		else if(!sm_PostponeProfileChange && sm_ProfileChangeActionCounter < 0)
		{
			// reset this to check again later...
			sm_ProfileChangeActionCounter = GetProfileChangeActionDelayMs();
			gnetDebug1("UpdatePendingProfileChange :: Resetting action check to %dms", GetProfileChangeActionDelayMs());
		}
	}
}

void CLiveManager::Update(const unsigned curTime)
{
	if(!sm_IsInitialized)
		return;

	// make sure we're not exhausting the rline heap.
	gnetAssert(sm_RlineAllocator->GetMemoryAvailable() >= (0.05f * sm_RlineAllocator->GetHeapSize()));

	const int timeStep = sm_LastUpdateTime ? int(curTime - sm_LastUpdateTime) : 0;
	sm_LastUpdateTime = curTime;

	// update any delayed profile change actions
	NPROFILE(CLiveManager::UpdatePendingProfileChange(timeStep));

	// update tracking related to being in / out if multiplayer
	NPROFILE(CLiveManager::UpdateMultiplayerTracking());

	const bool isSortingFriends = NetworkInterface::IsGameInProgress() && rlFriendsManager::IsSortingFriends();

	// update any deferred service event actions
	NPROFILE(UpdatePendingServiceActions())

	if(sm_SocketMgr)
	{
		NPROFILE(sm_SocketMgr->Update());
	}

	NPROFILE(netUpdate())
	NPROFILE(rlUpdate())

#if RSG_DURANGO || RSG_ORBIS
	sm_FindDisplayName.Update();
#endif

	if(!PARAM_nonetwork.Get())
	{
		// if we lose our network connection while in multiplayer then bail.
        if(!netHardware::IsLinkConnected())
        {
            if(ShouldBailFromMultiplayer())
            {
                gnetDebug1("Update :: Hardware is offline! Bailing from multiplayer!");
                NPROFILE(CLiveManager::BailOnMultiplayer(BAIL_PROFILE_CHANGE, BAIL_CTX_PROFILE_CHANGE_HARDWARE_OFFLINE))
            }

            if(ShouldBailFromVoiceSession())
            {
                gnetDebug1("Update :: Hardware is offline! Bailing from voice session!");
                NPROFILE(CLiveManager::BailOnVoiceSession())
            }
        }

		NPROFILE(g_SystemUi.Update())
		NPROFILE(sm_InviteMgr.Update(timeStep))
		NPROFILE(sm_PartyInviteMgr.Update(timeStep))
		NPROFILE(RefreshFriendsRead())

		// update friends count in match if list sort has completed
		if(isSortingFriends && !rlFriendsManager::IsSortingFriends())
		{
			NPROFILE(CLiveManager::RefreshFriendCountInMatch())
		}

#if RSG_XBOX
		NPROFILE(UpdatePrivilegeChecks())
		NPROFILE(UpdatePermissionChecks())
#endif

#if RSG_XDK
		if(sm_OverallReputationIsBadNeedsRefreshed && !sm_OverallReputationIsBadRefreshing)
		{
			sm_OverallReputationIsBadNeedsRefreshed = false;
			if(IsOnline())
			{
				gnetDebug1("Update :: Refreshing user reputation statistic");

				sm_OverallReputationIsBadStatistic.SetName("OverallReputationIsBad");
				NPROFILE(IUserStatistics::GetUserStatistics(NetworkInterface::GetLocalGamerIndex(), &sm_OverallReputationIsBadStatistic, 1, SERVICE_ID_GLOBAL_REPUTATION, &sm_StatisticsStatus))

				sm_OverallReputationIsBadRefreshing = true; 
			}
		}

		if(sm_OverallReputationIsBadRefreshing && !sm_StatisticsStatus.Pending())
		{
			sm_OverallReputationIsBadRefreshing = false;
			if(sm_StatisticsStatus.Succeeded())
			{
				sm_OverallReputationIsBad = (sm_OverallReputationIsBadStatistic.GetS64() != 0);
				gnetDebug1("Update :: User reputation: %s", sm_OverallReputationIsBad ? "Bad" : "Good");
			}
			else
			{
				gnetError("Update :: Failed to get user reputation with error 0x%08x", sm_StatisticsStatus.GetResultCode());
				sm_OverallReputationIsBad = false;
			}
		}
#endif

#if RSG_XBOX
		if(sm_bAvoidListNeedsRefreshed && !sm_bAvoidListRefreshing)
		{
			sm_bAvoidListNeedsRefreshed = false;
			if(IsOnline())
			{
				gnetDebug1("Update :: Requesting Avoid List");
				sm_bAvoidListRefreshing = true;
				g_rlXbl.GetPlayerManager()->GetAvoidList(NetworkInterface::GetLocalGamerIndex(), &sm_AvoidList, &sm_AvoidListStatus);
			}
		}

		if(sm_bAvoidListRefreshing && !sm_AvoidListStatus.Pending())
		{
			gnetDebug1("Update :: Avoid List :: %s - %d entries", sm_AvoidListStatus.Succeeded() ? "Retrieved" : "Failed", sm_AvoidList.m_Players.GetCount());
			sm_bAvoidListRefreshing = false;
			sm_bHasAvoidList = sm_AvoidListStatus.Succeeded(); 

			// update some systems directly
			CNetwork::GetVoice().OnAvoidListRetrieved();
		}
#endif

#if RSG_ORBIS
		if(sm_bPendingAppLaunchScriptEvent && CNetwork::IsScriptReadyForEvents())
		{
			gnetDebug1("Update :: Scripts ready. Firing pending app launched event.");
			GetEventScriptNetworkGroup()->Add(CEventNetworkAppLaunched(sm_nPendingAppLaunchFlags));
			sm_bPendingAppLaunchScriptEvent = false;
		}
#endif

		// Update it here so the playerCard messages are sent even if we're not in the pause menu
		if(SCPlayerCardCoronaManager::IsInstantiated())
			SCPlayerCardCoronaManager::GetInstance().Update();

		NPROFILE(sm_AchMgr.Update())
		NPROFILE(sm_RichPresenceMgr.Update())
		NPROFILE(NETWORK_CLAN_INST.Update())
		NPROFILE(sm_NetworkClanDataMgr.Update())
		NPROFILE(GetSocialClubMgr().Update(timeStep))
		NPROFILE(sm_ProfileStatsMgr.Update())
#if RL_FACEBOOK_ENABLED
		NPROFILE(sm_Facebook.Update())
#endif // #if RL_FACEBOOK_ENABLED
		NPROFILE(sm_scGamerDataMgr.Update())
		NPROFILE(CloudManager::GetInstance().Update())
#if defined(GTA_REPLAY) && GTA_REPLAY	
		NPROFILE(VideoUploadManager::GetInstance().Update())
#endif
		NPROFILE(UserContentManager::GetInstance().Update())
		NPROFILE(sm_Commerce.Update())
		NPROFILE(NetworkSCPresenceUtil::Update())
		NPROFILE(SC_INBOX_MGR.Update())
		NPROFILE(NETWORK_LEGAL_RESTRICTIONS.Update())
		NPROFILE(GamePresenceEvents::Update())
		NPROFILE(NETWORK_GAMERS_ATTRS.Update())
		NPROFILE(CNetworkNewsStoryMgr::Get().Update())
		NPROFILE(CNetworkEmailMgr::Get().Update())
#if GEN9_STANDALONE_ENABLED
        NPROFILE(CSocialClubFeedMgr::Get().Update())
        NPROFILE(CSocialClubFeedTilesMgr::Get().Update())
#endif
		NPROFILE(CNetworkTransitionNewsController::Get().Update())
		NPROFILE(SocialClubEventMgr::Get().Update())
		NPROFILE(SPolicyVersions::GetInstance().Update())
		NET_SHOP_ONLY(NPROFILE(NETWORK_SHOPPING_MGR.Update()))
		NET_SHOP_ONLY(NPROFILE(GameTransactionSessionMgr::Get().Update(curTime)))

#if RSG_SCE
		NPROFILE(UpdatePlayStationPlusEvent());
#endif

		NPROFILE(UpdateMembership());
		
#if NET_ENABLE_MEMBERSHIP_FUNCTIONALITY
		// players can stand-by and remain in-session for long periods so allow for a refresh every sm_EvaluateBenefitsInterval
		if(!sm_NeedsToEvaluateBenefits && (sm_EvaluateBenefitsInterval > 0) && ((sysTimer::GetSystemMsTime() - sm_LastEvaluateBenefitsTime) > sm_EvaluateBenefitsInterval))
		{
			netDebug("Update :: EvaluateBenefits refresh flagged");
			sm_NeedsToEvaluateBenefits = true;
		}

		// we want to consume benefits once when we are first able to and then again if flagged
		if(sm_NeedsToEvaluateBenefits && sm_Commerce.CanConsumeBenefits())
		{
			netDebug("Update :: Calling EvaluateSubscriptionBenefits");
			rlScMembership::EvaluateSubscriptionBenefits(NetworkInterface::GetLocalGamerIndex());
			sm_NeedsToEvaluateBenefits = false;
			sm_LastEvaluateBenefitsTime = sysTimer::GetSystemMsTime();
		}
#endif
	}

	NPROFILE(BANK_ONLY(CLiveManager::LiveBank_Update();))
}

#if RSG_ORBIS
void CLiveManager::UpdatePlayStationPlusEvent()
{
	static bool s_HasActivePlayStationPlusEvent = false;

	if(!SocialClubEventMgr::Get().GetDataReceived())
		return;

	if(!Tunables::GetInstance().HasCloudTunables())
		return;

	bool bRefreshPromotionChecks = false;

	const bool bHasPlayStationPlusPromotionEvent = HasActivePlayStationPlusEvent();
	if(bHasPlayStationPlusPromotionEvent != s_HasActivePlayStationPlusEvent)
	{
		gnetDebug1("UpdatePlayStationPlusEvent :: HasActivePlayStationPlusEvent: %s -> %s", s_HasActivePlayStationPlusEvent ? "True" : "False", bHasPlayStationPlusPromotionEvent ? "True" : "False");
		bRefreshPromotionChecks = true;
	}
	s_HasActivePlayStationPlusEvent = bHasPlayStationPlusPromotionEvent;

	// wait until we have a valid local index before updating anything 
	if(!RL_IS_VALID_LOCAL_GAMER_INDEX(NetworkInterface::GetLocalGamerIndex()))
		return;

	// PlayStation Plus promotion
	const bool bIsPlusRequiredForRealtimeMultiplayer = g_rlNp.IsPlayStationPlusRequiredForRealtimeMultiplayer(NetworkInterface::GetLocalGamerIndex());
	const bool bHasActivePlusPromotion = bHasPlayStationPlusPromotionEvent && IsPlayStationPlusPromotionEnabled();

	// if we no longer have a promotion, cleanly manage the exit from multiplayer / requirement reset
	if(!bHasActivePlusPromotion && !g_rlNp.IsPlayStationPlusRequiredForRealtimeMultiplayer(NetworkInterface::GetLocalGamerIndex()))
	{
		if(!NetworkInterface::IsNetworkOpen())
		{
			// if we're no longer in multiplayer, we can just update the requirement
			gnetDebug1("UpdatePlayStationPlusEvent :: Ending Promotion, in SP");
			g_rlNp.SetPlayStationPlusRequiredForRealtimeMultiplayer(NetworkInterface::GetLocalGamerIndex(), true);
		}
		else
		{
			// update graceful exit from multiplayer
			const unsigned graceMs = CLiveManager::GetPlayStationPlusPromotionGraceMs();
			if((graceMs > 0) && ((sysTimer::GetSystemMsTime() - graceMs) > sm_OnlinePrivilegePromotionEndedTime[NetworkInterface::GetLocalGamerIndex()]))
			{
				gnetDebug1("UpdatePlayStationPlusEvent :: Ending Promotion, Grace Period (%ums) Expired", graceMs);
				g_rlNp.SetPlayStationPlusRequiredForRealtimeMultiplayer(NetworkInterface::GetLocalGamerIndex(), true);

				// if we don't have PS+, bail with message
				if(!HasPlatformSubscription() && ShouldBailFromMultiplayer())
				{
					gnetDebug1("OnPresenceEvent :: Bailing from multiplayer.");
					CLiveManager::BailOnMultiplayer(BAIL_ONLINE_PRIVILEGE_REVOKED, BAIL_CTX_PRIVILEGE_PROMOTION_ENDED);
				}
			}
		}
	}

	// if nothing has changed, bail out
	if(!bRefreshPromotionChecks)
		return;

	// determine whether to update or not (we only do this immediately for a positive change or status quo)
	if(bHasActivePlusPromotion || bIsPlusRequiredForRealtimeMultiplayer)
	{
		gnetDebug1("UpdatePlayStationPlusEvent :: Updating PS+ Required for Multiplayer: %s -> %s", 
			g_rlNp.IsPlayStationPlusRequiredForRealtimeMultiplayer(NetworkInterface::GetLocalGamerIndex()) ? "True" : "False",
			!bHasActivePlusPromotion ? "True" : "False");
 
		g_rlNp.SetPlayStationPlusRequiredForRealtimeMultiplayer(NetworkInterface::GetLocalGamerIndex(), !bHasActivePlusPromotion);
	}

	// mark when the promotion ended if it ended (reset otherwise)
	sm_OnlinePrivilegePromotionEndedTime[NetworkInterface::GetLocalGamerIndex()] = (bHasActivePlusPromotion ? 0 : sysTimer::GetSystemMsTime());

	// log PS+ states here
	gnetDebug1("UpdatePlayStationPlusEvent :: Has PS+: %s", IsPlatformSubscriptionCheckPending() ? "Pending" : (HasPlatformSubscription() ? "True" : "False"));
	gnetDebug1("UpdatePlayStationPlusEvent :: Active PS+ Event: %s", HasActivePlayStationPlusEvent() ? "True" : "False");
	gnetDebug1("UpdatePlayStationPlusEvent :: PS+ Promotion Enabled: %s", IsPlayStationPlusPromotionEnabled() ? "True" : "False");
	gnetDebug1("UpdatePlayStationPlusEvent :: PS+ Custom Upsell Enabled: %s", IsPlayStationPlusCustomUpsellEnabled() ? "True" : "False");
	gnetDebug1("UpdatePlayStationPlusEvent :: PS+ Promotion Grace Period: %ums", GetPlayStationPlusPromotionGraceMs());
	gnetDebug1("UpdatePlayStationPlusEvent :: PS+ Loading Button Enabled: %s", NetworkInterface::DoesTunableDisablePauseStoreRedirect() ? "False" : "True");
	gnetDebug1("UpdatePlayStationPlusEvent :: PS+ Entry Prompt Enabled: %s", NetworkInterface::DoesTunableDisablePlusEntryPrompt() ? "False" : "True");
	gnetDebug1("UpdatePlayStationPlusEvent :: PS+ Pause Store Redirect: %s", NetworkInterface::DoesTunableDisablePauseStoreRedirect() ? "False" : "True");

	// Ensure to refresh bHasOnlinePrivileges after changing any promotion flags
	RefreshPrivileges();
}

bool CLiveManager::HasActivePlayStationPlusEvent()
{
	return
		// need both an active event and a tunable enabling the event
		(SocialClubEventMgr::Get().HasActivePlayStationPlusPromotion() NOTFINAL_ONLY(|| PARAM_netHasActivePlusEvent.Get())) &&
		(Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("PLUS_EVENT_IS_ENABLED", 0xf5b368d5), false) NOTFINAL_ONLY(|| PARAM_netPlusEventEnabled.Get()));
}

int CLiveManager::GetActivePlayStationPlusEventId()
{
	return SocialClubEventMgr::Get().GetPlayStationPlusPromotionEventId();
}

bool CLiveManager::IsPlayStationPlusCustomUpsellEnabled()
{
	return
		HasActivePlayStationPlusEvent() &&
		(Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("PLUS_EVENT_CUSTOM_UPSELL_ENABLED", 0x41a20cc5), false) NOTFINAL_ONLY(|| PARAM_netPlusCustomUpsellEnabled.Get()));
}

bool CLiveManager::IsPlayStationPlusPromotionEnabled()
{
	return
		HasActivePlayStationPlusEvent() &&
		(Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("PLUS_EVENT_PROMOTION_ENABLED", 0xd249ebf6), false) NOTFINAL_ONLY(|| PARAM_netPlusPromotionEnabled.Get()));
}

unsigned CLiveManager::GetPlayStationPlusPromotionGraceMs()
{
	static const unsigned DEFAULT_PLUS_EVENT_PROMOTION_GRACE_MS = 0;

	int plusPromotionGraceMs = DEFAULT_PLUS_EVENT_PROMOTION_GRACE_MS;
	if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("PLUS_EVENT_PROMOTION_GRACE_MS", 0xc51e978c), plusPromotionGraceMs))
		return static_cast<unsigned>(plusPromotionGraceMs);

#if !__FINAL
	if(PARAM_netPlusEventPromotionEndGraceMs.Get(plusPromotionGraceMs))
		return static_cast<unsigned>(plusPromotionGraceMs);
#endif

	return static_cast<unsigned>(plusPromotionGraceMs);
}
#endif

void CLiveManager::RefreshActivePlayer()
{
	int nMainPlayerIndex = CControlMgr::GetMainPlayerIndex();

#if RSG_PC
	// PC only supports player index 0 - regardless of which controller is used
	nMainPlayerIndex = 0;
#endif

	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if(localGamerIndex != nMainPlayerIndex)
	{
		if(nMainPlayerIndex >= 0 && rlPresence::IsSignedIn(nMainPlayerIndex))
		{
			rlGamerInfo gamerInfo;
			gnetVerify(rlPresence::GetGamerInfo(nMainPlayerIndex, &gamerInfo));

			gnetDebug1("RefreshActivePlayer :: Index: %d -> %d, Name: %s -> %s", localGamerIndex, gamerInfo.GetLocalIndex(), rlPresence::GetActingGamerInfo().GetName(), gamerInfo.GetName());
			CLiveManager::SetMainGamerInfo(gamerInfo);
		}
		else
		{
			CLiveManager::ClearMainGamerInfo();
		}
	}
}

void CLiveManager::InitProfile()
{
	gnetDebug1("InitProfile");
	RefreshActivePlayer();

	if(!SPolicyVersions::IsInstantiated())
	{
		SPolicyVersions::Instantiate();
	}

	if(NetworkInterface::GetActiveGamerInfo() && NetworkInterface::GetActiveGamerInfo()->IsOnline())
	{
		SPolicyVersions::GetInstance().Shutdown();
		SPolicyVersions::GetInstance().Init();
	}
}

void CLiveManager::ShutdownProfile()
{
	gnetDebug1("ShutdownProfile");
	RefreshActivePlayer();
}

bool CLiveManager::IsPermissionsValid(int nGamerIndex, const bool bCheckHasPrivilege)
{
	if(nGamerIndex == GAMER_INDEX_LOCAL)
	{
		nGamerIndex = (int)NetworkInterface::GetLocalGamerIndex();
	}
	else if(nGamerIndex == GAMER_INDEX_ANYONE)
	{
		for(unsigned i = 0; i < RL_MAX_LOCAL_GAMERS; i++)
		{
			if(bCheckHasPrivilege == sm_bPermissionsValid[i])
				return true;
		}
		return false;
	}

	return (nGamerIndex >= 0 && nGamerIndex < RL_MAX_LOCAL_GAMERS) ? sm_bPermissionsValid[nGamerIndex] : false;
}

bool CLiveManager::CheckIsAgeRestricted(int nGamerIndex)
{
#if !RSG_FINAL
    int nSetting = 0;
    if(PARAM_netPrivilegesAgeRestricted.Get(nSetting))
        return nSetting != 0;
#endif

	if(nGamerIndex == GAMER_INDEX_EVERYONE)
	{
		for(unsigned i = 0; i < RL_MAX_LOCAL_GAMERS; i++)
		{
			if(!sm_bIsRestrictedByAge[i])
				return false;
		}
		return true;
	}
	else if(nGamerIndex == GAMER_INDEX_LOCAL)
	{
		return RL_IS_VALID_LOCAL_GAMER_INDEX(NetworkInterface::GetLocalGamerIndex()) ? sm_bIsRestrictedByAge[NetworkInterface::GetLocalGamerIndex()] : false;
	}
	else if (nGamerIndex == GAMER_INDEX_ANYONE)
	{
		for(unsigned i = 0; i < RL_MAX_LOCAL_GAMERS; i++)
		{
			if(sm_bIsRestrictedByAge[i])
				return true;
		}
		return false;
	}
	else if(VALID_GAMER_INDEX_OR_TYPE(nGamerIndex))
	{
		return sm_bIsRestrictedByAge[nGamerIndex];
	}

	return false;
}

bool CLiveManager::CheckOnlinePrivileges(int nGamerIndex, const bool bCheckHasPrivilege)
{
#if !RSG_FINAL
	int nSetting = 0;
	if(PARAM_netPrivilegesMultiplayer.Get(nSetting))
		return nSetting != 0;

	if(NetworkInterface::LanSessionsOnly())
		return true;
#endif

	if(nGamerIndex == GAMER_INDEX_EVERYONE)
	{
		for(unsigned i = 0; i < RL_MAX_LOCAL_GAMERS; i++)
		{
			if(sm_bConsiderForPrivileges[i] && (bCheckHasPrivilege != sm_bHasOnlinePrivileges[i]))
				return false;
		}
		return true;
	}
	else if(nGamerIndex == GAMER_INDEX_LOCAL)
	{
		return RL_IS_VALID_LOCAL_GAMER_INDEX(NetworkInterface::GetLocalGamerIndex()) ? sm_bHasOnlinePrivileges[NetworkInterface::GetLocalGamerIndex()] : false;
	}
	else if(nGamerIndex == GAMER_INDEX_ANYONE)
	{
		for(unsigned i = 0; i < RL_MAX_LOCAL_GAMERS; i++)
		{
			if(sm_bConsiderForPrivileges[i] && (bCheckHasPrivilege == sm_bHasOnlinePrivileges[i]))
				return true;
		}
		return false;
	}
	else if(VALID_GAMER_INDEX_OR_TYPE(nGamerIndex))
	{
		return sm_bHasOnlinePrivileges[nGamerIndex];
	}

	return false;
}

bool CLiveManager::CheckUserContentPrivileges(const int nGamerIndex, const bool bCheckHasPrivilege)
{
#if !RSG_FINAL
	int nSetting = 0;
	if(PARAM_netPrivilegesUserContent.Get(nSetting))
		return nSetting != 0;
#endif

	if(nGamerIndex == GAMER_INDEX_EVERYONE)
	{
		for(unsigned i = 0; i < RL_MAX_LOCAL_GAMERS; i++)
		{
			if(sm_bConsiderForPrivileges[i] && (bCheckHasPrivilege != sm_bHasUserContentPrivileges[i]))
				return false;
		}
		return true;
	}
	else if(nGamerIndex == GAMER_INDEX_LOCAL)
	{
		return RL_IS_VALID_LOCAL_GAMER_INDEX(NetworkInterface::GetLocalGamerIndex()) ? sm_bHasUserContentPrivileges[NetworkInterface::GetLocalGamerIndex()] : false;
	}
	else if(nGamerIndex == GAMER_INDEX_ANYONE)
	{
		for(unsigned i = 0; i < RL_MAX_LOCAL_GAMERS; i++)
		{
			if(sm_bConsiderForPrivileges[i] && (bCheckHasPrivilege == sm_bHasUserContentPrivileges[i]))
				return true;
		}
		return false;
	}
	else if(VALID_GAMER_INDEX_OR_TYPE(nGamerIndex))
	{
		return sm_bHasUserContentPrivileges[nGamerIndex];
	}

	return false;
}

bool CLiveManager::CheckVoiceCommunicationPrivileges(const int nGamerIndex, const bool bCheckHasPrivilege)
{
#if !RSG_FINAL
	int nSetting = 0;
	if(PARAM_netPrivilegesVoiceCommunication.Get(nSetting))
		return nSetting != 0;
#endif

	if(nGamerIndex == GAMER_INDEX_EVERYONE)
	{
		for(unsigned i = 0; i < RL_MAX_LOCAL_GAMERS; i++)
		{
			if(sm_bConsiderForPrivileges[i] && (bCheckHasPrivilege != sm_bHasVoiceCommunicationPrivileges[i]))
				return false;
		}
		return true;
	}
	else if(nGamerIndex == GAMER_INDEX_LOCAL)
	{
		return RL_IS_VALID_LOCAL_GAMER_INDEX(NetworkInterface::GetLocalGamerIndex()) ? sm_bHasVoiceCommunicationPrivileges[NetworkInterface::GetLocalGamerIndex()] : false;
	}
	else if(nGamerIndex == GAMER_INDEX_ANYONE)
	{
		for(unsigned i = 0; i < RL_MAX_LOCAL_GAMERS; i++)
		{
			if(sm_bConsiderForPrivileges[i] && (bCheckHasPrivilege == sm_bHasVoiceCommunicationPrivileges[i]))
				return true;
		}
		return false;
	}
	else if(VALID_GAMER_INDEX_OR_TYPE(nGamerIndex))
	{
		return sm_bHasVoiceCommunicationPrivileges[nGamerIndex];
	}

	return false;
}

bool CLiveManager::CheckTextCommunicationPrivileges(const int nGamerIndex, const bool bCheckHasPrivilege)
{
#if !RSG_FINAL
	int nSetting = 0;
	if(PARAM_netPrivilegesTextCommunication.Get(nSetting))
		return nSetting != 0;
#endif

	if(nGamerIndex == GAMER_INDEX_EVERYONE)
	{
		for(unsigned i = 0; i < RL_MAX_LOCAL_GAMERS; i++)
		{
			if(sm_bConsiderForPrivileges[i] && (bCheckHasPrivilege != sm_bHasTextCommunicationPrivileges[i]))
				return false;
		}
		return true;
	}
	else if(nGamerIndex == GAMER_INDEX_LOCAL)
	{
		return RL_IS_VALID_LOCAL_GAMER_INDEX(NetworkInterface::GetLocalGamerIndex()) ? sm_bHasTextCommunicationPrivileges[NetworkInterface::GetLocalGamerIndex()] : false;
	}
	else if(nGamerIndex == GAMER_INDEX_ANYONE)
	{
		for(unsigned i = 0; i < RL_MAX_LOCAL_GAMERS; i++)
		{
			if(sm_bConsiderForPrivileges[i] && (bCheckHasPrivilege == sm_bHasTextCommunicationPrivileges[i]))
				return true;
		}
		return false;
	}
	else if(VALID_GAMER_INDEX_OR_TYPE(nGamerIndex))
	{
		return sm_bHasTextCommunicationPrivileges[nGamerIndex];
	}

	return false;
}

bool CLiveManager::GetSocialNetworkingSharingPrivileges(const int nGamerIndex)
{
#if !RSG_FINAL
	int nSetting = 0;
	if(PARAM_netPrivilegesSocialNetworkSharing.Get(nSetting))
		return nSetting != 0;
#endif

	if(nGamerIndex == GAMER_INDEX_EVERYONE)
	{
		for(unsigned i = 0; i < RL_MAX_LOCAL_GAMERS; i++)
		{
			if(sm_bHasSocialNetSharePriv[i])
				return false;
		}
		return true;
	}
	else if(nGamerIndex == GAMER_INDEX_LOCAL)
	{
		return RL_IS_VALID_LOCAL_GAMER_INDEX(NetworkInterface::GetLocalGamerIndex()) ? sm_bHasSocialNetSharePriv[NetworkInterface::GetLocalGamerIndex()] : false;
	}
	else if(nGamerIndex == GAMER_INDEX_ANYONE)
	{
		for(unsigned i = 0; i < RL_MAX_LOCAL_GAMERS; i++)
		{
			if(sm_bHasSocialNetSharePriv[i])
				return true;
		}
		return false;
	}
	else if(VALID_GAMER_INDEX_OR_TYPE(nGamerIndex))
	{
		return sm_bHasSocialNetSharePriv[nGamerIndex];
	}

	return false;
}

bool
CLiveManager::HasRosPrivilege(const rlRosPrivilegeId privilegeId, const int nGamerIndex /* = GAMER_INDEX_LOCAL */)
{
	if(!NetworkInterface::HasValidRosCredentials(nGamerIndex))
		return false;

	if(nGamerIndex == GAMER_INDEX_EVERYONE)
	{
		for(unsigned i = 0; i < RL_MAX_LOCAL_GAMERS; i++)
		{
			if(!rlRos::HasPrivilege(i, privilegeId))
				return false;
		}
		return true;
	}
	else if(nGamerIndex == GAMER_INDEX_LOCAL)
	{
		return RL_IS_VALID_LOCAL_GAMER_INDEX(NetworkInterface::GetLocalGamerIndex()) ? rlRos::HasPrivilege(NetworkInterface::GetLocalGamerIndex(), privilegeId) : false;
	}
	else if(nGamerIndex == GAMER_INDEX_ANYONE)
	{
		for(unsigned i = 0; i < RL_MAX_LOCAL_GAMERS; i++)
		{
			if(rlRos::HasPrivilege(i, privilegeId))
				return true;
		}
		return false;
	}
	else if(VALID_GAMER_INDEX_OR_TYPE(nGamerIndex))
	{
		return rlRos::HasPrivilege(nGamerIndex, privilegeId);
	}

	return false;
}

rlAgeGroup CLiveManager::GetAgeGroup()
{
	return rlGamerInfo::GetAgeGroup(NetworkInterface::GetLocalGamerIndex());
}

bool CLiveManager::HadOnlinePermissionsPromotionWithinMs(const unsigned nWithinMs, int nGamerIndex)
{
	if(nGamerIndex == GAMER_INDEX_LOCAL)
	{
		nGamerIndex = NetworkInterface::GetLocalGamerIndex();
	}
	else if(nGamerIndex == GAMER_INDEX_ANYONE)
	{
		for(unsigned i = 0; i < RL_MAX_LOCAL_GAMERS; i++)
		{
			if((sm_OnlinePrivilegePromotionEndedTime[i] != 0) && ((nWithinMs == 0) || ((sysTimer::GetSystemMsTime() - sm_OnlinePrivilegePromotionEndedTime[i]) < nWithinMs)))
				return true;
		}
		return false;
	}

	if(RL_IS_VALID_LOCAL_GAMER_INDEX(nGamerIndex))
	{
		return (sm_OnlinePrivilegePromotionEndedTime[nGamerIndex] != 0) && ((nWithinMs == 0) || ((sysTimer::GetSystemMsTime() - sm_OnlinePrivilegePromotionEndedTime[nGamerIndex]) < nWithinMs));
	}
	return false;
}

bool CLiveManager::HasRosCredentialsResult(int nGamerIndex)
{
    if(nGamerIndex == GAMER_INDEX_LOCAL)
	{
		nGamerIndex = (int)NetworkInterface::GetLocalGamerIndex();
	}
    else if(nGamerIndex == GAMER_INDEX_ANYONE)
    {
        for(unsigned i = 0; i < RL_MAX_LOCAL_GAMERS; i++)
        {
            if(sm_bHasCredentialsResult[i])
                return true; 
        }
        return false;
    }

	return (nGamerIndex >= 0 && nGamerIndex < RL_MAX_LOCAL_GAMERS) ? sm_bHasCredentialsResult[nGamerIndex] : false;
}

bool CLiveManager::IsGuest(int nGamerIndex)
{
	if(nGamerIndex == GAMER_INDEX_LOCAL)
	{
		nGamerIndex = (int)NetworkInterface::GetLocalGamerIndex();
	}
	else if(nGamerIndex == GAMER_INDEX_ANYONE)
	{
		for(unsigned i = 0; i < RL_MAX_LOCAL_GAMERS; i++)
		{
			if(rlPresence::IsGuest(i))
				return true; 
		}
		return false;
	}

	return (nGamerIndex >= 0 && nGamerIndex < RL_MAX_LOCAL_GAMERS) ? rlPresence::IsGuest(nGamerIndex) : false;
}

bool CLiveManager::IsSignedIn(int nGamerIndex)
{
	if(nGamerIndex == GAMER_INDEX_LOCAL)
	{
		nGamerIndex = (int)NetworkInterface::GetLocalGamerIndex();
	}
	else if(nGamerIndex == GAMER_INDEX_ANYONE)
	{
		for(unsigned i = 0; i < RL_MAX_LOCAL_GAMERS; i++)
		{
			if(rlPresence::IsSignedIn(i))
				return true; 
		}
		return false;
	}

	return (nGamerIndex >= 0 && nGamerIndex < RL_MAX_LOCAL_GAMERS) ? rlPresence::IsSignedIn(nGamerIndex) : false;
}

bool CLiveManager::IsOnline(int nGamerIndex)
{
	if(nGamerIndex == GAMER_INDEX_LOCAL)
	{
		nGamerIndex = (int)NetworkInterface::GetLocalGamerIndex();
	}
	else if(nGamerIndex == GAMER_INDEX_ANYONE)
	{
		for(unsigned i = 0; i < RL_MAX_LOCAL_GAMERS; i++)
		{
			if(rlPresence::IsOnline(i))
				return true; 
		}
		return false;
	}

	return (nGamerIndex >= 0 && nGamerIndex < RL_MAX_LOCAL_GAMERS) ? rlPresence::IsOnline(nGamerIndex) : false;
}

bool CLiveManager::IsOnlineRos(int nGamerIndex)
{
    if(nGamerIndex == GAMER_INDEX_LOCAL)
	{
		nGamerIndex = (int)NetworkInterface::GetLocalGamerIndex();
	}
    else if(nGamerIndex == GAMER_INDEX_ANYONE)
    {
        for(unsigned i = 0; i < RL_MAX_LOCAL_GAMERS; i++)
        {
            if(sm_bIsOnlineRos[i])
                return true; 
        }
        return false;
    }
       
	return (nGamerIndex >= 0 && nGamerIndex < RL_MAX_LOCAL_GAMERS) ? sm_bIsOnlineRos[nGamerIndex] : false;
}

bool CLiveManager::HasValidRosCredentials(int nGamerIndex)
{
    if(nGamerIndex == GAMER_INDEX_LOCAL)
	{
		if(NetworkInterface::LanSessionsOnly())
		{
			return true;
		}

		nGamerIndex = (int)NetworkInterface::GetLocalGamerIndex();
	}
    else if(nGamerIndex == GAMER_INDEX_ANYONE)
    {
        for(unsigned i = 0; i < RL_MAX_LOCAL_GAMERS; i++)
        {
            if(sm_bHasValidCredentials[i])
                return true; 
        }
        return false;
    }

    return (nGamerIndex >= 0 && nGamerIndex < RL_MAX_LOCAL_GAMERS) ? sm_bHasValidCredentials[nGamerIndex] : false;
}

bool CLiveManager::HasCredentialsResult(int nGamerIndex)
{
    if(nGamerIndex == GAMER_INDEX_LOCAL)
	{
		if(NetworkInterface::LanSessionsOnly())
		{
			return true;
		}

		nGamerIndex = (int)NetworkInterface::GetLocalGamerIndex();
	}
    else if(nGamerIndex == GAMER_INDEX_ANYONE)
    {
        for(unsigned i = 0; i < RL_MAX_LOCAL_GAMERS; i++)
        {
            if(sm_bHasCredentialsResult[i])
                return true; 
        }
        return false;
    }

    return (nGamerIndex >= 0 && nGamerIndex < RL_MAX_LOCAL_GAMERS) ? sm_bHasCredentialsResult[nGamerIndex] : false;
}

bool CLiveManager::IsRefreshingRosCredentials(int nGamerIndex)
{
	if(nGamerIndex == GAMER_INDEX_LOCAL)
	{
		nGamerIndex = (int)NetworkInterface::GetLocalGamerIndex();
	}
	else if(nGamerIndex == GAMER_INDEX_ANYONE)
	{
		for(unsigned i = 0; i < RL_MAX_LOCAL_GAMERS; i++)
		{
			if(rlRos::GetLoginStatus(i)==RLROS_LOGIN_STATUS_IN_PROGRESS)
				return true; 
		}
		return false;
	}

	return nGamerIndex >= 0 ? rlRos::GetLoginStatus(nGamerIndex)==RLROS_LOGIN_STATUS_IN_PROGRESS : false;
}

bool CLiveManager::HasValidRockstarId(int nGamerIndex)
{
    if(nGamerIndex == GAMER_INDEX_LOCAL)
	{
		nGamerIndex = (int)NetworkInterface::GetLocalGamerIndex();
	}
    else if(nGamerIndex == GAMER_INDEX_ANYONE)
    {
        for(unsigned i = 0; i < RL_MAX_LOCAL_GAMERS; i++)
        {
            if(sm_bHasValidRockstarId[i])
                return true; 
        }
        return false;
    }

    return (nGamerIndex >= 0 && nGamerIndex < RL_MAX_LOCAL_GAMERS) ? sm_bHasValidRockstarId[nGamerIndex] : false;
}

#if RSG_XBOX
static const int INVALID_PERMISSIONS_ID = -1; 
static const unsigned MAX_PERMISSIONS_CHECKS = 10; 

struct PermissionsCheck
{
	PermissionsCheck() { Clear(); }

	void Clear() 
	{
		m_PermissionCheckID = INVALID_PERMISSIONS_ID;
		m_PermissionStatus.Reset();
	}

	bool IsFree()
	{
		return (m_PermissionCheckID == INVALID_PERMISSIONS_ID);
	}

	PrivacySettingsResult m_PermissionResult;
	netStatus m_PermissionStatus;
	int m_PermissionCheckID;
};

static PermissionsCheck g_PermissionChecks[MAX_PERMISSIONS_CHECKS];
static int g_PermissionCheckID = 0;

bool CLiveManager::ResolvePlatformPrivilege(const int localGamerIndex, const rlPrivileges::PrivilegeTypes privilegeType, const bool waitForSystemUi)
{
	int localGamerIndexToUse = RL_INVALID_GAMER_INDEX;
	if(localGamerIndex == GAMER_INDEX_LOCAL)
	{
		localGamerIndexToUse = NetworkInterface::GetLocalGamerIndex();
	}
	else if(VALID_GAMER_INDEX_OR_TYPE(localGamerIndex))
	{
		localGamerIndexToUse = static_cast<int>(localGamerIndex);
	}

	gnetDebug1("ResolvePlatformPrivilege :: Index: %d, Privilege: %d", localGamerIndex, privilegeType);

	if(!RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
	{
		return false;
	}

	if(waitForSystemUi && g_SystemUi.IsUiShowing())
	{
		gnetDebug1("ResolvePlatformPrivilege :: Waiting for SystemUI");
		sm_bPendingPrivilegeCheckForUi = true;
		sm_PendingPrivilegeType = privilegeType;
		sm_PendingPrivilegeLocalGamerIndex = localGamerIndex;
	}
	else
	{
		gnetDebug1("ResolvePlatformPrivilege :: Checking");

		// We don't know which game system was already checking privileges.
		// This is usually an indicator of a double-request. We'll fail with a warning..
		if(!rlPrivileges::IsPrivilegeCheckInProgress())
		{
			// Passing true here will show Xbox UI error handling screens
			rlPrivileges::CheckPrivileges(localGamerIndex, rlPrivileges::GetPrivilegeBit(privilegeType), true);

			// mark as a fire-and-forget request
			rlPrivileges::SetPrivilegeCheckResultNotNeeded();
		}
		else
		{
			gnetDebug1("ResolvePlatformPrivilege :: An XBL privilege check is already in progress - Waiting for Idle");
			sm_bPendingPrivilegeCheckForUi = true;
			sm_PendingPrivilegeType = privilegeType;
			sm_PendingPrivilegeLocalGamerIndex = localGamerIndex;
		}
	}
	return true;
}
 
int CLiveManager::StartPermissionsCheck(IPrivacySettings::ePermissions nPermission, const rlGamerHandle& hGamer, const int nGamerIndex)
{
	unsigned nLocalGamerIndex = nGamerIndex;
	if(nLocalGamerIndex == GAMER_INDEX_LOCAL)
	{
		if(!IsSignedIn())
			return INVALID_PERMISSIONS_ID;

		nLocalGamerIndex = NetworkInterface::GetLocalGamerIndex();
	}

	// find a free permissions check
	int nIndex = INVALID_PERMISSIONS_ID;
	for(int i = 0; i < MAX_PERMISSIONS_CHECKS; i++)
	{
		if(g_PermissionChecks[i].IsFree())
		{
			nIndex = i;
			break;
		}
	}

	// check we got one
	if(!gnetVerify(nIndex != INVALID_PERMISSIONS_ID))
	{
		gnetError("StartPermissionsCheck :: No free permissions check slots! Maximum: %d", MAX_PERMISSIONS_CHECKS);
		return INVALID_PERMISSIONS_ID;
	}

	// we now exclusively use the block list for this permission
	if(nPermission == IPrivacySettings::PERM_PlayMultiplayer)
	{
		// keep the interface consistent
		g_PermissionChecks[nIndex].m_PermissionStatus.SetPending();
		g_PermissionChecks[nIndex].m_PermissionStatus.SetSucceeded();
		g_PermissionChecks[nIndex].m_PermissionResult.m_isAllowed = !IsInAvoidList(hGamer);
		return g_PermissionChecks[nIndex].m_PermissionCheckID;
	}
	else if(nPermission == IPrivacySettings::PERM_ViewTargetUserCreatedContent)
	{
		// if we have full privileges, complete immediately (with allowed)
		if(CLiveManager::CheckUserContentPrivileges())
		{
			g_PermissionChecks[nIndex].m_PermissionStatus.SetPending();
			g_PermissionChecks[nIndex].m_PermissionStatus.SetSucceeded();
			g_PermissionChecks[nIndex].m_PermissionResult.m_isAllowed = true; 
			return g_PermissionChecks[nIndex].m_PermissionCheckID;
		}
		// or if this gamer is in our avoid / block list (with not allowed)
		else if(IsInAvoidList(hGamer))
		{
			g_PermissionChecks[nIndex].m_PermissionStatus.SetPending();
			g_PermissionChecks[nIndex].m_PermissionStatus.SetSucceeded();
			g_PermissionChecks[nIndex].m_PermissionResult.m_isAllowed = false; 
			return g_PermissionChecks[nIndex].m_PermissionCheckID;
		}
		// or if the gamer is not our friend (only friends will pass from here)
		else if(!rlFriendsManager::IsFriendsWith(nLocalGamerIndex, hGamer))
		{
			g_PermissionChecks[nIndex].m_PermissionStatus.SetPending();
			g_PermissionChecks[nIndex].m_PermissionStatus.SetSucceeded();
			g_PermissionChecks[nIndex].m_PermissionResult.m_isAllowed = false; 
			return g_PermissionChecks[nIndex].m_PermissionCheckID;
		}
	}

	// run a service check if we reached this far
	bool bSuccess = IPrivacySettings::CheckPermission(nLocalGamerIndex, nPermission, hGamer, &g_PermissionChecks[nIndex].m_PermissionResult, &g_PermissionChecks[nIndex].m_PermissionStatus);
	if(bSuccess)
	{
		g_PermissionChecks[nIndex].m_PermissionCheckID = g_PermissionCheckID++;
		gnetDebug3("StartPermissionsCheck :: ID: %d", g_PermissionChecks[nIndex].m_PermissionCheckID); 
		return g_PermissionChecks[nIndex].m_PermissionCheckID;
	}
	else
	{
		gnetError("StartPermissionsCheck :: CheckPermission Failed"); 
		return INVALID_PERMISSIONS_ID;
	}
}

void CLiveManager::UpdatePrivilegeChecks()
{
	if(sm_bPendingPrivilegeCheckForUi && !g_SystemUi.IsUiShowing() && !rlPrivileges::IsPrivilegeCheckInProgress())
	{
		gnetDebug3("UpdatePrivilegeChecks :: Issuing privilege check...");

		// Passing true here will show Xbox UI error handling screens
		rlPrivileges::CheckPrivileges(sm_PendingPrivilegeLocalGamerIndex, rlPrivileges::GetPrivilegeBit(sm_PendingPrivilegeType), true);

		// mark as a fire-and-forget request
		rlPrivileges::SetPrivilegeCheckResultNotNeeded();
		
		// reset pending flag
		sm_bPendingPrivilegeCheckForUi = false;
	}
}

void CLiveManager::UpdatePermissionChecks()
{
	for(int i = 0; i < MAX_PERMISSIONS_CHECKS; i++) if(!g_PermissionChecks[i].IsFree())
	{
		if(!g_PermissionChecks[i].m_PermissionStatus.Pending())
		{
			gnetDebug3("UpdatePermissionChecks :: %s - ID: %d, Allowed: %s", g_PermissionChecks[i].m_PermissionStatus.Succeeded() ? "Succeeded" : "Failed", g_PermissionChecks[i].m_PermissionCheckID, g_PermissionChecks[i].m_PermissionResult.m_isAllowed ? "True" : "False");

			// generate event for script
			GetEventScriptNetworkGroup()->Add(CEventNetworkPermissionCheckResult(g_PermissionChecks[i].m_PermissionCheckID, g_PermissionChecks[i].m_PermissionStatus.Succeeded() && g_PermissionChecks[i].m_PermissionResult.m_isAllowed));

			// remove this check
			g_PermissionChecks[i].Clear();
		}
	}
}
#endif

unsigned CLiveManager::GetValidSentFromGroupsFromTunables(const unsigned defaultGroups, const char* tunableStub)
{
	// exit if tunable stub is null
	if(!tunableStub)
		return defaultGroups;

	// need to build the tunable name
	char tunableName[RAGE_MAX_PATH];

	// capture the default
	bool allowReceiveFromAll = (defaultGroups & SentFromGroup::Group_All) == SentFromGroup::Group_All;
	Tunables::GetInstance().Access(CD_GLOBAL_HASH, atStringHash(formatf(tunableName, "NET_%s_ALLOW_RECEIVE_FROM_ALL", tunableStub)), allowReceiveFromAll);
	
	// if this is true, we can early out here
	if(allowReceiveFromAll)
	{
		gnetDebug1("GetValidSentFromGroupsFromTunables :: %s - Returning Group_All", tunableStub);
		return SentFromGroup::Group_All;
	}

	unsigned groupSetting = 0;
	for(int i = 0; i < SentFromGroup::Group_Num; i++)
	{
		// get our bit and work out the default
		const unsigned sentFromGroup = (1 << i);
		bool allowReceiveFromGroup = (defaultGroups & sentFromGroup) == sentFromGroup;

		// build a tunable name - use upper case for consistency
		atString tunableStubString(tunableStub); tunableStubString.Uppercase();
		atString groupString(NetworkUtils::GetSentFromGroupStub(sentFromGroup)); groupString.Uppercase();
		formatf(tunableName, "NET_%s_ALLOW_RECEIVE_FROM_%s", tunableStubString.c_str(), groupString.c_str());

		// pull the tunable
		Tunables::GetInstance().Access(CD_GLOBAL_HASH, atStringHash(tunableName), allowReceiveFromGroup);

		// add this to our group settings
		if(allowReceiveFromGroup)
		{
			gnetDebug1("GetValidSentFromGroupsFromTunables :: %s - Allow: %s", tunableStub, groupString.c_str());
			groupSetting |= (1 << i);
		}
	}

	return groupSetting;
}

bool CLiveManager::CanReceiveFrom(const rlGamerHandle& sender, rlClanId crewId, const unsigned validSentFromGroups OUTPUT_ONLY(, const char* logStub))
{
	// Group_Invalid receives special treatment
	if(!sender.IsValid())
	{
		const bool allowInvalid = (validSentFromGroups & SentFromGroup::Group_Invalid) != 0;
		gnetDebug1("CanReceiveFrom :: %s - %s: Group_Invalid", logStub, allowInvalid ? "Allow" : "Deny");
		return allowInvalid;
	}

	// Group_NotSameSession receives special treatment - this explicitly prevents any of the below checks from
	// passing for players in the same session
	if((validSentFromGroups & SentFromGroup::Group_NotSameSession) != 0 && NetworkInterface::GetPlayerFromGamerHandle(sender) != nullptr)
	{
		gnetDebug1("CanReceiveFrom :: %s - Deny: Group_NotSameSession", logStub);
		return false;
	}

	// special case for the local player - this should be an explicit allow or deny
	// to prevent the local player returning true out of any of our further checks (i.e. same session, same team)
	if(NetworkInterface::GetLocalGamerHandle() == sender)
	{
		// check local player
		if((validSentFromGroups & SentFromGroup::Group_LocalPlayer) != 0)
		{
			gnetDebug1("CanReceiveFrom :: %s - Allow: Group_LocalPlayer", logStub);
			return true;
		}
		else
		{
			gnetDebug1("CanReceiveFrom :: %s - Deny: Local Sender but Group_LocalPlayer not allowed", logStub);
			return false;
		}
	}

	// early exit if we can receive from anyone
	if(validSentFromGroups == SentFromGroup::Group_All)
	{
		gnetDebug1("CanReceiveFrom :: %s - Allow: Group_All", logStub);
		return true;
	}

	// special case for the local player - this should be an explicit allow or deny
	// to prevent the local player returning true out of any of our further checks (i.e. same session, same team)
	if(NetworkInterface::GetLocalGamerHandle() == sender)
	{
		// check local player
		if((validSentFromGroups & SentFromGroup::Group_LocalPlayer) != 0)
		{
			gnetDebug1("CanReceiveFrom :: %s - Allow: Group_LocalPlayer", logStub);
			return true;
		}
		else
		{
			gnetDebug1("CanReceiveFrom :: %s - Deny: Local Sender but Group_LocalPlayer not allowed", logStub);
			return false;
		}
	}

	// check friends
	if(((validSentFromGroups & SentFromGroup::Group_Friends) != 0) && rlFriendsManager::IsFriendsWith(NetworkInterface::GetLocalGamerIndex(), sender))
	{
		gnetDebug1("CanReceiveFrom :: %s - Allow: Group_Friends", logStub);
		return true;
	}

	// check recent players
	if(((validSentFromGroups & SentFromGroup::Group_RecentPlayer) != 0) && CNetwork::GetNetworkSession().GetRecentPlayers().IsRecentPlayer(sender))
	{
		gnetDebug1("CanReceiveFrom :: %s - Allow: Group_RecentPlayer", logStub);
		return true;
	}

	// these checks all rely on being in a physical network game
	if(NetworkInterface::IsGameInProgress())
	{
		// check same session
		if(((validSentFromGroups & SentFromGroup::Group_SameSession) != 0) && NetworkInterface::GetPlayerFromGamerHandle(sender) != nullptr)
		{
			gnetDebug1("CanReceiveFrom :: %s - Allow: Group_SameSession", logStub);
			return true;
		}

		// check same team
		if((validSentFromGroups & SentFromGroup::Group_SameTeam) != 0)
		{
			// only valid in a team restricted mode
			if(CNetwork::GetVoice().IsTeamOnlyChat())
			{
				CNetGamePlayer* localPlayer = NetworkInterface::GetLocalPlayer();
				CNetGamePlayer* remotePlayer = NetworkInterface::GetPlayerFromGamerHandle(sender);
				if (localPlayer &&
					remotePlayer &&
					localPlayer->GetTeam() >= 0 &&
					remotePlayer->GetTeam() >= 0 &&
					localPlayer->GetTeam() == remotePlayer->GetTeam())
				{
					gnetDebug1("CanReceiveFrom :: %s - Allow: Group_SameTeam", logStub);
					return true;
				}
			}
		}
	}

	// check crew (we only allow for our primary crew)
	if(CLiveManager::GetNetworkClan().HasPrimaryClan() && ((validSentFromGroups & SentFromGroup::Group_LargeCrew) != 0 || (validSentFromGroups & SentFromGroup::Group_SmallCrew) != 0))
	{
		// check crew - must be primary 
		const rlClanDesc* primaryCrew = CLiveManager::GetNetworkClan().GetPrimaryClan();
		
		// work out if this is a large crew 
		const bool isLargeCrew = sm_LargeCrewThreshold > 0 && primaryCrew->m_MemberCount > sm_LargeCrewThreshold; 
		
		// reject if we have a large crew but don't allow large crews
		const bool rejectLargeCrew = isLargeCrew && (validSentFromGroups & SentFromGroup::Group_LargeCrew) == 0;
		
		// if we can proceed
		if(!rejectLargeCrew)
		{
			// if we have not been provided with a crew Id, look this player up and get it from there
			if(crewId == RL_INVALID_CLAN_ID)
			{
				CNetGamePlayer* remotePlayer = NetworkInterface::GetPlayerFromGamerHandle(sender);
				if(remotePlayer)
					crewId = remotePlayer->GetCrewId();
			}

			// check again...
			if(crewId != RL_INVALID_CLAN_ID && crewId == primaryCrew->m_Id)
			{
				gnetDebug1("CanReceiveFrom :: %s - Allow: %s (Crew Matches)", logStub, isLargeCrew ? "Group_LargeCrew" : "Group_SmallCrew");
				return true;
			}

			// we can also check if the remote player is in the member view that we have for the crew
			if(CLiveManager::GetNetworkClan().IsMember(sender))
			{
				gnetDebug1("CanReceiveFrom :: %s - Allow: %s (Member)", logStub, isLargeCrew ? "Group_LargeCrew" : "Group_SmallCrew");
				return true;
			}
		}
	}

	gnetDebug1("CanReceiveFrom :: %s - Deny: No valid groups", logStub);
	return false;
}

void CLiveManager::UpdatePendingServiceActions()
{
	if(sm_bSuspendActionPending)
	{
		gnetDebug1("UpdatePendingServiceActions :: Processing deferred suspend action");
		sm_bSuspendActionPending = false;
		sm_bGameWasSuspended = true; 

		// trigger network event
		GetEventScriptNetworkGroup()->Add(CEventNetworkSystemServiceEvent(sysServiceEvent::SUSPENDED, 0));

		// cancel all pending net / rline tasks
		rlGetTaskManager()->CancelAll();
		netTask::CancelAll();

		// unadvertise all sessions
		if(rlScMatchManager::HasAnyMatches() && NetworkInterface::IsLocalPlayerSignedIn())
			rlScMatchmaking::UnadvertiseAll(NetworkInterface::GetLocalGamerIndex(), NULL);

		// we need to send multiplayer events here (in case we won't be resuming)
		if(CNetwork::IsNetworkSessionValid())
			CNetwork::GetNetworkSession().OnServiceEventSuspend();
	}

	if(sm_bResumeActionPending)
	{
		gnetDebug1("UpdatePendingServiceActions :: Processing deferred resume action");
		
		// trigger network event
		GetEventScriptNetworkGroup()->Add(CEventNetworkSystemServiceEvent(sysServiceEvent::RESUMING, 0));

		// reset tracking variables
		sm_bResumeActionPending = false;
		sm_bGameWasSuspended = false;

		// cancel in-flight invite
		sm_InviteMgr.ClearInvite(InviteMgr::CF_Force);

        // handle suspend for rlXbl internals
        XBOX_ONLY(rlXbl::HandleSuspend());

		// check if we should bail from multiplayer
		if(ShouldBailFromMultiplayer())
		{
			gnetDebug1("UpdatePendingServiceActions :: Resuming. Bailing from Multiplayer");
			CLiveManager::BailOnMultiplayer(BAIL_SYSTEM_SUSPENDED, BAIL_CTX_SUSPENDED_ON_RESUME);
		}
#if RSG_ORBIS || RSG_DURANGO
		else
		{
			/*
			    //////////////////////////////////////////////////////////////////////////
				* Fix a bunch of exploits related to suspending the console before 
				* entering multi player on ps4.
				* Stats are loaded with old values and when entering MP we don't load the
				* data already changed on another console.
				* for that reason we manage to keep the cash and all data 
				* changed because we only flush stats that have changed in this console.
				//////////////////////////////////////////////////////////////////////////
			*/
			bool exploitCheckOnResume = true;
			Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CHECK_EXPLOIT_ON_RESUME", 0x17572711), exploitCheckOnResume);
			if (exploitCheckOnResume)
			{
				CStatsSavesMgr& savemgr = CStatsMgr::GetStatsDataMgr().GetSavesMgr();
				if (CStatsMgr::GetStatsDataMgr().GetAllOnlineStatsAreSynched() || !savemgr.IsLoadPending(STAT_MP_CATEGORY_DEFAULT))
				{
					gnetError("UpdatePendingServiceActions :: Resuming. ALL STATS ARE SYNCHED - FORCE RELOAD OF ALL STATS.");

					CStatsMgr::GetStatsDataMgr().ResynchServerAuthoritative(true);
					CLiveManager::GetProfileStatsMgr().ForceMPProfileStatsGetFromCloud();

					savemgr.ClearDirtyRead();
					savemgr.ClearPendingFlushRequests(true);
					for (int slot = 0; slot <= STAT_MP_CATEGORY_MAX; slot++)
					{
						savemgr.SetLoadPending(slot);
						savemgr.ClearLoadFailed(slot);
					}
				}
			}
		}
#endif //RSG_ORBIS || RSG_DURANGO

		if(ShouldBailFromVoiceSession())
		{
			gnetDebug1("UpdatePendingServiceActions :: Resuming. Bailing from Voice Session");
			CLiveManager::BailOnVoiceSession();
		}

#if RSG_ORBIS
		// on PS4, after resuming, we'll get a netHardware reset if we get socket errors due
		// to SCE_NET_EINACTIVEDISABLED, indicating an IP release has occurred
		// we don't get a suspended notification like XBO, so mirror the initial calls here to 
		// wipe out all tasks (these will all fail)

		// cancel all pending net / rline tasks
		rlGetTaskManager()->CancelAll();
		netTask::CancelAll();

		// unadvertise all sessions
		if(rlScMatchManager::HasAnyMatches() && NetworkInterface::IsLocalPlayerSignedIn())
			rlScMatchmaking::UnadvertiseAll(NetworkInterface::GetLocalGamerIndex(), NULL);

		if(sm_bFlagIpReleasedOnGameResume)
		{
			gnetDebug1("UpdatePendingServiceActions :: Notify IP release socket error");
			netHardware::NotifyIpReleaseSocketError();
		}
#endif
	}
}

bool
CLiveManager::ShowSigninUi(unsigned nSignInFlags)
{
	gnetAssert(sm_IsInitialized);

#if RSG_PC
	if (IsSignedIn())
	{
		gnetWarning("Attempting to show signin UI while already signed in");

		bool success = false;

		if (g_rlPc.IsUiAcceptingCommands())
		{
			success = g_rlPc.ShowUi();
		}

		return success;
	}
#endif

	return g_SystemUi.ShowSigninUi(CControlMgr::GetMainPlayerIndex(), nSignInFlags);
}

bool
CLiveManager::ShowGamerProfileUi(const rlGamerHandle& target)
{
	gnetAssert(sm_IsInitialized);

	if(gnetVerify(NetworkInterface::IsLocalPlayerSignedIn()))
    {
    	return g_SystemUi.ShowGamerProfileUi(NetworkInterface::GetLocalGamerIndex(), target);
    }
	else
	{
		return false;
	}
}

void
CLiveManager::ShowGamerFeedbackUi(const rlGamerHandle& target)
{
	gnetAssert(sm_IsInitialized);

	if (gnetVerify(NetworkInterface::IsLocalPlayerSignedIn()))
	{
    	g_SystemUi.ShowGamerFeedbackUi(NetworkInterface::GetLocalGamerIndex(), target);
    }
}

void 
CLiveManager::ShowMessageComposeUI(const rlGamerHandle* pRecipients, unsigned nRecipients, const char* szSubject, const char* szMessage)
{
	gnetAssert(sm_IsInitialized);

	if (gnetVerify(NetworkInterface::IsLocalPlayerSignedIn()))
	{
#if !__NO_OUTPUT
		gnetDebug1("ShowMessageComposeUI :: Sending to %d gamers", nRecipients);
		for(int i = 0; i < nRecipients; i++)
			gnetDebug1("\tShowMessageComposeUI :: Sending to %s", NetworkUtils::LogGamerHandle(pRecipients[i]));
#endif

		g_SystemUi.ShowMessageComposeUi(NetworkInterface::GetLocalGamerIndex(), pRecipients, nRecipients, szSubject, szMessage);
	}
}

bool CLiveManager::HasPlatformSubscription(const bool initialCheckOnly)
{
	gnetAssert(sm_IsInitialized);

	// track if this is called before we ever receive a platform check result
	if(IsPlatformSubscriptionCheckPending(initialCheckOnly))
	{
		gnetAssertf(0, "HasPlatformSubscription :: Not completed initial subscription check!");
	}

	if(gnetVerify(NetworkInterface::IsLocalPlayerSignedIn()))
	{
#if RSG_ORBIS
		return rlGamerInfo::HasPlayStationPlusSubscription(NetworkInterface::GetLocalGamerIndex());
#elif RSG_XBL || RSG_XENON || RSG_PC
		// tied to online privilege - no direct check for subscription
		return CheckOnlinePrivileges(NetworkInterface::GetLocalGamerIndex());
#endif
	}

	return false;
}

bool CLiveManager::IsPlatformSubscriptionCheckPending(const bool initialCheckOnly)
{
	gnetAssert(sm_IsInitialized);

	static bool s_HasInitialPlatformSubscriptionResult = false;

	bool isCheckPending = false;

	if(gnetVerify(NetworkInterface::IsLocalPlayerSignedIn()))
	{
#if RSG_NP
		// wait for our PS+ check to come in...
		if(rlGamerInfo::IsPlaystationPlusCheckPending(NetworkInterface::GetLocalGamerIndex()))
			isCheckPending = true;
		// otherwise, if we don't have PS+, wait for the tunables / event file to see if we have an override...
		else if(!g_rlNp.GetNpAuth().IsPlusAuthorized(NetworkInterface::GetLocalGamerIndex()))
			isCheckPending = !SocialClubEventMgr::Get().GetDataReceived() || !Tunables::GetInstance().HasCloudTunables();
		else // we can check
			isCheckPending = false;
#elif RSG_XBL
		isCheckPending = rlPrivileges::IsRefreshPrivilegesPending(NetworkInterface::GetLocalGamerIndex());
#endif
	}

	// update our initial result cache if the checks have completed
	if(!isCheckPending && !s_HasInitialPlatformSubscriptionResult)
	{
		gnetDebug1("IsPlatformSubscriptionCheckPending :: Have Initial Result");
		s_HasInitialPlatformSubscriptionResult = true;
	}

	// if we are only interested in whether we have ever received a PS+ result, check that
	// this prevents any temporary downtime if we refresh the result mid-run
	if(initialCheckOnly && s_HasInitialPlatformSubscriptionResult)
		return false;

	return isCheckPending;
}

bool CLiveManager::IsSubAccount()
{
#if RSG_ORBIS
	gnetAssert(sm_IsInitialized);

	if(gnetVerify(NetworkInterface::IsLocalPlayerSignedIn()))
	{
		const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		if(HasValidRosCredentials(localGamerIndex))
		{
			return rlGamerInfo::IsPlaystationSubAccount(localGamerIndex);
		}
	}
#endif
	return false;
}

bool CLiveManager::IsAccountSignedOutOfOnlineService()
{
#if RSG_ORBIS
	gnetAssert(sm_IsInitialized);

	if(gnetVerify(NetworkInterface::IsLocalPlayerSignedIn()))
	{
		return rlGamerInfo::IsSignedOutOfPSN(NetworkInterface::GetLocalGamerIndex());
	}
#endif
	return false;
}

bool
CLiveManager::ShowAccountUpgradeUI(const unsigned RL_NP_DEEP_LINK_MANUAL_CLOSE_ONLY(nDeepLinkBrowserCloseTime))
{
	gnetAssert(sm_IsInitialized);

    // local player must be signed in to show upsell
    if(!gnetVerify(NetworkInterface::IsLocalPlayerSignedIn()))
        return false;

#if RSG_SCE
	const bool isOnline = NetworkInterface::IsLocalPlayerOnline();
	gnetDebug1("ShowAccountUpgradeUI :: Custom: %s, Online: %s", IsPlayStationPlusCustomUpsellEnabled() ? "True" : "False", isOnline ? "True" : "False");

	// must be online to see the custom upsell
    if(isOnline && IsPlayStationPlusCustomUpsellEnabled())
    {
        static const unsigned CUSTOM_PLUS_BROWSER_HEADER_WIDTH = 1920;
        static const unsigned CUSTOM_PLUS_BROWSER_BODY_WIDTH = 1920;
        static const unsigned CUSTOM_PLUS_BROWSER_BODY_HEIGHT = 1080;
        static const unsigned CUSTOM_PLUS_BROWSER_POS_Y = 0;

        rlSystemBrowserConfig config;
		config.m_DialogMode = rlSystemBrowserConfig::DM_CUSTOM;
		config.m_ViewOptions = rlSystemBrowserConfig::VIEW_OPTION_TRANSPARENT;
        config.m_HeaderWidth = CUSTOM_PLUS_BROWSER_HEADER_WIDTH;
        config.m_Width = CUSTOM_PLUS_BROWSER_BODY_WIDTH;
        config.m_Height = CUSTOM_PLUS_BROWSER_BODY_HEIGHT;
		config.m_PosY = CUSTOM_PLUS_BROWSER_POS_Y;
		config.m_Animate = false;
        config.m_Flags = rlSystemBrowserConfig::FLAG_DEEP_LINK_URL | rlSystemBrowserConfig::FLAG_PLATFORM_SUBSCRIPTION_CHECK;

#if RL_NP_DEEP_LINK_MANUAL_CLOSE
		// needed only on V SDK browser
		config.m_DeepLinkCloseTime = nDeepLinkBrowserCloseTime;
#endif

        // add an exit button (should be hidden)
        if(Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("PLUS_EVENT_UPSELL_HAS_EXIT_BUTTON", 0x04980ad3), true))
            config.m_Controls = rlSystemBrowserConfig::CTRL_EXIT;

        // get the index of out 
        const int plusPromotionEventId = GetActivePlayStationPlusEventId();

		// get our URL data
		char SIEE_URL[RAGE_MAX_PATH];     bool bValid_SIEE_URL = false;
		char SIEA_URL[RAGE_MAX_PATH];     bool bValid_SIEA_URL = false;
		char SIEJ_URL[RAGE_MAX_PATH];     bool bValid_SIEJ_URL = false;
		char SIEAsia_URL[RAGE_MAX_PATH];  bool bValid_SIEAsia_URL = false;
		{
			bValid_SIEE_URL = SocialClubEventMgr::Get().GetExtraEventData(plusPromotionEventId, "storeUrlEU", SIEE_URL);
			bValid_SIEA_URL = SocialClubEventMgr::Get().GetExtraEventData(plusPromotionEventId, "storeUrlUs", SIEA_URL);
			bValid_SIEJ_URL = SocialClubEventMgr::Get().GetExtraEventData(plusPromotionEventId, "storeUrlJPN", SIEJ_URL);
			bValid_SIEAsia_URL = SocialClubEventMgr::Get().GetExtraEventData(plusPromotionEventId, "storeUrlAsia", SIEAsia_URL);
		}

		// for custom urls
		char CUSTOM_URL[RAGE_MAX_PATH];

		// build tunable SKU override parameter
		char tunableParam[RAGE_MAX_PATH];
		formatf(tunableParam, "PLUS_EVENT_UPSELL_OVERRIDE_SKU_%s", sysAppContent::GetBuildLocaleCode());

		// this checks if we want to override the default (use country / region) behaviour for a particular SKU
		if (Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, atStringHash(tunableParam), false))
		{
			gnetDebug1("ShowAccountUpgradeUI :: SKU: %s (Tunable override)", sysAppContent::GetBuildLocaleString());
			if (sysAppContent::IsEuropeanBuild() && bValid_SIEE_URL) config.m_Url = SIEE_URL;
			else if (sysAppContent::IsAmericanBuild() && bValid_SIEA_URL) config.m_Url = SIEA_URL;
			else if (sysAppContent::IsJapaneseBuild() && bValid_SIEJ_URL) config.m_Url = SIEJ_URL;
			// we don't have an Asian SKU - the American SKU is sold in Asia (outside Japan)

#if !__NO_OUTPUT
			if (config.m_Url != nullptr)
			{
				gnetDebug1("ShowAccountUpgradeUI :: %s SKU - Using url: %s", sysAppContent::GetBuildLocaleString(), config.m_Url);
			}
#endif
		}
		// use the country code / region
		else
		{
			// work out which region's upsell to use
			rlNpSceRegion region = rlNpSceRegion::Region_Invalid;
			bool useCustomRegion = false;

			// get the country code
			char npCountryCode[3];
			g_rlNp.GetCountryCode(NetworkInterface::GetLocalGamerIndex(), npCountryCode);

			// check if we need to use a custom region for a specific country
			formatf(tunableParam, "PLUS_EVENT_UPSELL_USE_CUSTOM_REGION_%s", npCountryCode);
			if(Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, atStringHash(tunableParam), false))
			{
				gnetDebug1("ShowAccountUpgradeUI :: Custom Region - Country: %s", npCountryCode);

				// append custom in the event that our country code clashes with an existing region code
				char eventParam[RAGE_MAX_PATH];
				formatf(eventParam, "storeUrl%s_Custom", npCountryCode);

				// check for the custom path (this will need to be added ad-hoc to the event system on demand)
				useCustomRegion = SocialClubEventMgr::Get().GetExtraEventData(plusPromotionEventId, eventParam, CUSTOM_URL);
				if(useCustomRegion)
				{
					gnetDebug1("ShowAccountUpgradeUI :: Custom Region - Url: %s", CUSTOM_URL);
					config.m_Url = CUSTOM_URL;
				}
			}

			// if not using a custom region...
			if(!useCustomRegion)
			{
				// build tunable country override parameter
				formatf(tunableParam, "PLUS_EVENT_UPSELL_OVERRIDE_COUNTRY_%s", npCountryCode);

				// check if we're overriding our region for this country by tunable
				int regionOverride;
				if(Tunables::GetInstance().Access(CD_GLOBAL_HASH, atStringHash(tunableParam), regionOverride))
				{
					if(gnetVerify(regionOverride > rlNpSceRegion::Region_Invalid && regionOverride < rlNpSceRegion::Region_Max))
					{
						region = static_cast<rlNpSceRegion>(regionOverride);
						gnetDebug1("ShowAccountUpgradeUI :: Country: %s, Region: %s (Tunable Override)", npCountryCode, g_rlNp.GetSceRegionAsString(region));
					}
				}
				else
				{
					// use the region defined by SCE
					region = g_rlNp.GetSceRegion(NetworkInterface::GetLocalGamerIndex());
					gnetDebug1("ShowAccountUpgradeUI :: Country: %s, Region: %s", npCountryCode, g_rlNp.GetSceRegionAsString(region));
				}

				// make sure we picked a valid region
				if(region != rlNpSceRegion::Region_Invalid)
				{
					if(region == rlNpSceRegion::Region_SIEE && bValid_SIEE_URL) config.m_Url = SIEE_URL;
					else if(region == rlNpSceRegion::Region_SIEA && bValid_SIEA_URL) config.m_Url = SIEA_URL;
					else if(region == rlNpSceRegion::Region_SIEJ && bValid_SIEJ_URL) config.m_Url = SIEJ_URL;
					else if(region == rlNpSceRegion::Region_SIEAsia && bValid_SIEAsia_URL) config.m_Url = SIEAsia_URL;

#if !__NO_OUTPUT
					if(config.m_Url != nullptr)
					{
						gnetDebug1("ShowAccountUpgradeUI :: Region: %s, Url: %s", g_rlNp.GetSceRegionAsString(region), config.m_Url);
					}
#endif
				}
			}
		}

#if !__FINAL
		// if we haven't picked one in dev, use a developer facing test fallback
		if (config.m_Url == nullptr)
		{
			static const char* CUSTOM_PLUS_BROWSER_URI = "psns:browse?category=IP9102-NPIA90006_00-PLUSTEST";
			gnetDebug1("ShowAccountUpgradeUI :: Using development test URL: %s", CUSTOM_PLUS_BROWSER_URI);
			config.m_Url = CUSTOM_PLUS_BROWSER_URI;
		}
#endif

		// check that we set the url to something valid
		if (config.m_Url != nullptr)
		{
			gnetDebug1("ShowAccountUpgradeUI :: Using custom upsell, Url: %s", config.m_Url);
			sm_LastBrowserUpsellRequestTimestamp = sysTimer::GetSystemMsTime();
			return g_SystemUi.ShowWebBrowser(NetworkInterface::GetLocalGamerIndex(), config);
		}
    }
	else
	{
		gnetDebug1("ShowAccountUpgradeUI :: Custom Upsell Disabled, using regular PS+ upsell");
	}
#endif
	
    return g_SystemUi.ShowAccountUpgradeUI(NetworkInterface::GetLocalGamerIndex());
}

void CLiveManager::ShowAppHelpUI()
{
	if(gnetVerify(NetworkInterface::IsLocalPlayerSignedIn()))
	{
		g_SystemUi.ShowAppHelpMenu(NetworkInterface::GetLocalGamerIndex());
	}
}

bool
CLiveManager::ShowPlatformPartyUi()
{
#if RSG_XDK
	gnetAssert(sm_IsInitialized);
	if(gnetVerify(NetworkInterface::IsLocalPlayerSignedIn()))
	{
		return g_SystemUi.ShowPartySessionsUi(NetworkInterface::GetLocalGamerIndex());
	}
	else
	{
		return false;
	}
#else
	return false;
#endif // RSG_XDK
}

bool 
CLiveManager::SendPlatformPartyInvites()
{
#if RSG_XDK
	gnetAssert(sm_IsInitialized);

	if(gnetVerify(NetworkInterface::IsLocalPlayerSignedIn()))
	{
		return g_SystemUi.ShowSendInviteUi(NetworkInterface::GetLocalGamerIndex());
	}
	else
	{
		return false;
	}
#else
	return false;
#endif // RSG_XDK
}

bool CLiveManager::IsInPlatformParty()
{
#if RSG_ORBIS
	gnetAssert(sm_IsInitialized);
	if(!gnetVerifyf(NetworkInterface::IsLocalPlayerOnline(), "IsInPlatformParty :: Local player not online!"))
		return false;

	return g_rlNp.GetNpParty().IsInParty();
#elif RSG_XDK
	gnetAssert(sm_IsInitialized);
	if(!gnetVerifyf(NetworkInterface::IsLocalPlayerOnline(), "IsInPlatformParty :: Local player not online!"))
		return false;

	return g_rlXbl.GetPartyManager()->GetPartySize() > 0;
#else
	return false;
#endif // RSG_ORBIS
}

int CLiveManager::GetPlatformPartyMemberCount()
{
#if RSG_ORBIS
	gnetAssert(sm_IsInitialized);
	return g_rlNp.GetNpParty().GetPartySize();
#elif RSG_XDK
	gnetAssert(sm_IsInitialized);
	return g_rlXbl.GetPartyManager()->GetPartySize();
#else
	return 0;
#endif
}

unsigned CLiveManager::GetPlatformPartyInfo(rlPlatformPartyMember* PARTY_PLATFORMS_ONLY(pMembers), unsigned PARTY_PLATFORMS_ONLY(nMembers))
{
#if RSG_ORBIS
	gnetAssert(sm_IsInitialized);
	if(!gnetVerifyf(NetworkInterface::IsLocalPlayerOnline(), "GetPlatformPartyInfo :: Local player not online!"))
		return false;

	gnetAssertf(nMembers >= MAX_NUM_PARTY_MEMBERS, "GetPlatformPartyInfo :: Increase size of members array!");

	rlNpPartyMemberInfo	members[SCE_NP_PARTY_MEMBER_NUM_MAX];
	int numMembers = g_rlNp.GetNpParty().GetPartyMembers(members);

	for (int i = 0; i< numMembers; i++)
	{
		sysMemCpy(&pMembers[i].m_MemberInfo, &members[i], sizeof(rlNpPartyMemberInfo));
	}

	return numMembers;
#elif RSG_DURANGO
	(void)nMembers;

	gnetAssert(sm_IsInitialized);
	if(!gnetVerifyf(NetworkInterface::IsLocalPlayerOnline(), "GetPlatformPartyInfo :: Local player not online!"))
		return false;

	gnetAssertf(nMembers >= MAX_NUM_PARTY_MEMBERS, "GetPlatformPartyInfo :: Increase size of members array!");

	int partySize = g_rlXbl.GetPartyManager()->GetPartySize();
	for (int i = 0; i < partySize; i++)
	{
		sysMemCpy(&pMembers[i].m_PartyInfo, g_rlXbl.GetPartyManager()->GetPartyMember(i), sizeof(rlXblPartyMemberInfo));
	}

	return partySize;
#else
	return 0;
#endif // RSG_ORBIS
}

bool CLiveManager::JoinPlatformPartyMemberSession(const rlGamerHandle& PARTY_PLATFORMS_ONLY(hGamer))
{
#if RSG_ORBIS
	gnetAssert(sm_IsInitialized);
	if(!gnetVerifyf(NetworkInterface::IsLocalPlayerOnline(), "GetPlatformPartyInfo :: Local player not online!"))
		return false;

	rlNpPartyMemberInfo	members[SCE_NP_PARTY_MEMBER_NUM_MAX];
	int numMembers = g_rlNp.GetNpParty().GetPartyMembers(members);

	rlGamerHandle hPartyMember;
	for (int i = 0; i< numMembers; i++)
	{
		rlSceNpOnlineId onlineId;
		onlineId.Reset(members[i].m_SceNpPartyMemberInfo.npId.handle);

		hPartyMember.ResetNp(g_rlNp.GetEnvironment(), RL_INVALID_NP_ACCOUNT_ID, &onlineId);
		if (hPartyMember == hGamer)
		{
			if (members[i].m_IsInSession)
			{
				rlSessionInfo sessionInfo;
				const rlGamerInfo* pInfo = NetworkInterface::GetActiveGamerInfo();

				// fake an invite accepted event
				CLiveManager::GetInviteMgr().HandleJoinRequest(sessionInfo, *pInfo, hGamer, InviteFlags::IF_ViaParty);

				// success
				return true; 
			}
		}
	}
	// not present in party
	return false;
#elif RSG_DURANGO
	gnetAssert(sm_IsInitialized);
	if(!gnetVerifyf(NetworkInterface::IsLocalPlayerOnline(), "GetPlatformPartyInfo :: Local player not online!"))
		return false;

	int partySize = g_rlXbl.GetPartyManager()->GetPartySize();
	for (int i = 0; i< partySize; i++)
	{
		rlXblPartyMemberInfo* pInfo;
		pInfo = g_rlXbl.GetPartyManager()->GetPartyMember(i);

		if (pInfo->m_GamerHandle == hGamer)
		{
			if (pInfo->m_IsInSession)
			{
				rlSessionInfo sessionInfo;
				const rlGamerInfo* pMyInfo = NetworkInterface::GetActiveGamerInfo();

				// fake an invite accepted event
				CLiveManager::GetInviteMgr().HandleJoinRequest(sessionInfo, *pMyInfo, hGamer, InviteFlags::IF_ViaParty);

				// success
				return true; 
			}
		}
	}
	// not present in party
	return false;
#else
	return false;
#endif // RSG_ORBIS
}

bool CLiveManager::IsInPlatformPartyChat()
{
#if RSG_XDK
    return g_rlXbl.GetPartyManager()->IsPartyChatActive();
#else
	return false;
#endif
}

bool CLiveManager::IsChattingInPlatformParty(const rlGamerHandle&)
{
    return false;
}

bool CLiveManager::ShowPlatformVoiceChannelUI(bool)
{
    return false;
}

void 
CLiveManager::AddPlayer(const rlGamerInfo& gamerInfo)
{
	gnetAssert(gamerInfo.IsValid());
	if(gamerInfo.IsRemote())
	{
		//Update number of players that are friends
		if (NetworkInterface::IsGameInProgress() && 
			rlFriendsManager::IsFriendsWith(NetworkInterface::GetLocalGamerIndex(), gamerInfo.GetGamerHandle()))
		{
			sm_FriendsInMatch++;

			//Update MOST_FRIENDS_IN_ONE_MATCH
			StatsInterface::SetGreater(STAT_MP_MOST_FRIENDS_IN_ONE_MATCH, (float)sm_FriendsInMatch);
		}
	}
}

void 
CLiveManager::RemovePlayer(const rlGamerInfo& gamerInfo)
{
	gnetAssert(gamerInfo.IsValid());

	if (gamerInfo.IsRemote())
	{
		//Update number of players that are friends
		if (NetworkInterface::IsGameInProgress() && 
			rlFriendsManager::IsFriendsWith(NetworkInterface::GetLocalGamerIndex(), gamerInfo.GetGamerHandle()))
		{
			sm_FriendsInMatch--;

			//Update MOST_FRIENDS_IN_ONE_MATCH
			StatsInterface::SetGreater(STAT_MP_MOST_FRIENDS_IN_ONE_MATCH, (float)sm_FriendsInMatch);
		}
	}
	else
	{
		CLiveManager::ResetFriendsInMatch();
	}
}

void
CLiveManager::RefreshFriendCountInMatch()
{
	sm_FriendsInMatch = 0;

	if (NetworkInterface::IsGameInProgress())
	{
		unsigned                 numRemoteActivePlayers = netInterface::GetNumRemoteActivePlayers();
		const netPlayer * const *remoteActivePlayers    = netInterface::GetRemoteActivePlayers();

		for(unsigned index = 0; index < numRemoteActivePlayers; index++)
		{
			const netPlayer *pPlayer = remoteActivePlayers[index];
			const rlGamerHandle& playerHandle = pPlayer->GetGamerInfo().GetGamerHandle();

			if (playerHandle.IsValid() && 
				rlFriendsManager::IsFriendsWith(NetworkInterface::GetLocalGamerIndex(), playerHandle))
			{
				sm_FriendsInMatch++;
			}
		}

		//Update MOST_FRIENDS_IN_ONE_MATCH
		StatsInterface::SetGreater(STAT_MP_MOST_FRIENDS_IN_ONE_MATCH, (float)sm_FriendsInMatch);
	}
}

#if RSG_XBOX
bool CLiveManager::IsInAvoidList(const rlGamerHandle& hGamer)
{
	return sm_AvoidList.m_Players.Find(hGamer.GetXuid()) >= 0;
}

void CLiveManager::ClearSignedInIntoNewProfileAfterSuspendMode()
{
	if(sm_SignedInIntoNewProfileAfterSuspendMode)
	{
		gnetDebug1("ClearSignedInIntoNewProfileAfterSuspendMode");
		sm_SignedInIntoNewProfileAfterSuspendMode = false;
	}
}
#endif

bool CLiveManager::IsInBlockList(const rlGamerHandle& hGamer)
{
#if RSG_XBOX
	return IsInAvoidList(hGamer);
#elif RSG_NP
	return g_rlNp.GetBasic().IsPlayerOnBlockList(hGamer);
#else
	(void)hGamer;
	return false;
#endif
}

void 
CLiveManager::ResetFriendsInMatch()
{
	sm_FriendsInMatch = 0;
}

bool 
CLiveManager::IsAddingFriend()
{
	return sm_AddFriendStatus.Pending();
}

bool 
CLiveManager::AddFriend(const rlGamerHandle& gamerHandle, const char* szMessage)
{
	(void)szMessage;
	gnetAssert(sm_IsInitialized);

	// check local player is online
	if(!gnetVerifyf(NetworkInterface::IsLocalPlayerOnline(), "AddFriend :: Local player not online!"))
		return false;

	// PS3 goes through presence, other platforms go through system UI
	return g_SystemUi.ShowAddFriendUI(NetworkInterface::GetLocalGamerIndex(), gamerHandle, szMessage);
}

bool CLiveManager::OpenEmptyFriendPrompt()
{
	return false;
}

bool CLiveManager::IsSystemUiShowing()
{
#if __DEV
	if (PARAM_nonetwork.Get())
	{
		return false;
	}
#endif

	gnetAssert(sm_IsInitialized);
	return g_SystemUi.IsUiShowing();
}

bool CLiveManager::HasRecentUpsellRequest(const unsigned recentThresholdMs)
{
	// if we have never requested an upsell, return false
	if(sm_LastBrowserUpsellRequestTimestamp == 0)
		return false;

	unsigned thresholdMs = recentThresholdMs;
	if(thresholdMs == 0)
	{
		static const int DEFAULT_RECENT_UPSELL_THRESHOLD = 2000;
		thresholdMs = DEFAULT_RECENT_UPSELL_THRESHOLD;

		if(Tunables::IsInstantiated())
			thresholdMs = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NET_RECENT_UPSELL_TIME_THRESHOLD", 0x0e26e810), DEFAULT_RECENT_UPSELL_THRESHOLD);
	}

	return (sysTimer::GetSystemMsTime() - sm_LastBrowserUpsellRequestTimestamp) < thresholdMs;
}

bool CLiveManager::IsOfflineInvitePending()
{
    return sm_bOfflineInvitePending;
}

void CLiveManager::ClearOfflineInvitePending()
{
    gnetDebug1("ClearOfflineInvitePending");
    sm_bOfflineInvitePending = false;
}

bool CLiveManager::HasBootInviteToProcess()
{
    return GetInviteMgr().HasBootInviteToProcess();
}

bool CLiveManager::ShouldSkipBootUi(OUTPUT_ONLY(bool logresult))
{
    #define LOG_BOOT_CHECK(txt) OUTPUT_ONLY( if (logresult) { gnetDebug1(txt); } )

	// check for a boot invite...
    if(GetInviteMgr().HasBootInviteToProcess(OUTPUT_ONLY(logresult)))
    {
		LOG_BOOT_CHECK("ShouldSkipBootUi :: Yes, pending boot invite");
        return true;
    }
    
	// We also check the currently accepted invite
	if(CLiveManager::IsSignedIn() && CLiveManager::GetInviteMgr().HasPendingAcceptedInvite())
    {
		LOG_BOOT_CHECK("ShouldSkipBootUi :: Yes, non-boot invite");
        return true;
    }

#if GEN9_STANDALONE_ENABLED
	// check for pending script router link...
	if(ScriptRouterLink::HasPendingScriptRouterLink())
	{
		ScriptRouterContextData contextData;
		if(ScriptRouterLink::ParseScriptRouterLink(contextData) && contextData.m_ScriptRouterSource.Int == eScriptRouterContextSource::SRCS_PROSPERO_GAME_INTENT)
		{
			LOG_BOOT_CHECK("ShouldSkipBootUi :: Yes, pending script router link via game intent");
			return true;
		}
	}
#endif

	LOG_BOOT_CHECK("ShouldSkipBootUi :: No");
    return false;
}

bool CLiveManager::SignInInviteeForBoot()
{
    return GetInviteMgr().SignInInviteeForBoot();
}

bool CLiveManager::FindGamersViaPresence(const char* szQuery, const char* pParams)
{
	// verify that we don't have an active task
	if(!gnetVerify(!sm_FindGamerTask))
	{
		gnetDebug1("FindGamersViaPresence :: Task is already active!");
		return false;
	}

    // verify that we are online
    if(!gnetVerify(IsOnline()))
    {
        gnetDebug1("FindGamersViaPresence :: Not Online!");
        return false;
    }

    // verify that we have credentials
    if(!gnetVerify(HasValidRosCredentials(NetworkInterface::GetLocalGamerIndex())))
    {
        gnetDebug1("FindGamersViaPresence :: No Ros Credentials!");
        return false;
    }

	// logging
	gnetDebug1("FindGamersViaPresence :: %s, %s", szQuery, pParams ? pParams : "No Params");

	// create a new task to handle this
	sm_FindGamerTask = rage_new FindGamerTask();
	sm_FindGamerTask->m_Status.Reset();
	sm_FindGamerTask->m_nGamers = 0;

	// run presence query
	return rlPresenceQuery::FindGamers(rlPresence::GetActingGamerInfo(), 
									 true, 
									 szQuery, 
									 pParams, 
									 sm_FindGamerTask->m_GamerData, 
									 MAX_FIND_GAMERS, 
									 &sm_FindGamerTask->m_nGamers, 
									 &sm_FindGamerTask->m_Status);
}

bool CLiveManager::IsFindingGamers()
{
	return sm_FindGamerTask && sm_FindGamerTask->m_Status.Pending();
}

bool CLiveManager::DidFindGamersSucceed()
{
	// verify that we don't have an active task
	if(!gnetVerify(sm_FindGamerTask))
	{
		gnetError("DidFindGamersSucceed :: Task does not exist!");
		return false;
	}

	return sm_FindGamerTask->m_Status.Succeeded();
}

unsigned CLiveManager::GetNumFoundGamers()
{
	// verify that we have an active task
	if(!gnetVerify(sm_FindGamerTask))
	{
		gnetError("GetNumFoundGamers :: Task does not exist!");
		return false;
	}

	return sm_FindGamerTask->m_nGamers;
}

bool CLiveManager::GetFoundGamer(int nIndex, rlGamerQueryData* pGamer)
{
	pGamer->Clear();

	// verify that we have an active task
	if(!gnetVerify(sm_FindGamerTask))
	{
		gnetError("GetFoundGamer :: Task does not exist!");
		return false;
	}

	// verify that we don't have an active task
	if(!gnetVerify(nIndex >= 0 && nIndex < sm_FindGamerTask->m_nGamers))
	{
		gnetError("GetFoundGamer :: Invalid index of %d!", nIndex);
		return false;
	}

	*pGamer = sm_FindGamerTask->m_GamerData[nIndex];
	return true;
}

rlGamerQueryData* CLiveManager::GetFoundGamers(unsigned* nNumGamers)
{
	*nNumGamers = sm_FindGamerTask->m_nGamers;
	return sm_FindGamerTask->m_GamerData;
}

void CLiveManager::ClearFoundGamers()
{
	// logging
	gnetDebug1("ClearFoundGamers");

	if(sm_FindGamerTask)
	{
        if(sm_FindGamerTask->m_Status.Pending())
        {
            gnetDebug1("ClearFoundGamers :: Cancelling in-flight task");
            netTask::Cancel(&sm_FindGamerTask->m_Status);
        }

		delete sm_FindGamerTask;
		sm_FindGamerTask = NULL;
	}
}

bool CLiveManager::GetGamerStatus(rlGamerHandle* pGamers, unsigned nGamers)
{
	gnetDebug1("GetGamerStatus :: For %d gamers", nGamers);

	// verify that we don't have an active task
	if(!gnetVerify(!sm_GetGamerStatusTask))
	{
		gnetError("GetGamerStatus :: Task is already active!");
		return false;
	}

    // verify that we are online
    if(!gnetVerify(IsOnline()))
    {
        gnetDebug1("GetGamerStatus :: Not Online!");
        return false;
    }

    // verify that we have credentials
	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
    if(!gnetVerify(HasValidRosCredentials(localGamerIndex)))
    {
        gnetDebug1("GetGamerStatus :: No Ros Credentials!");
        return false;
    }

	// verify that we haven't been given too many gamers
	if(!gnetVerify(nGamers < MAX_GAMER_STATUS))
	{
		gnetError("GetGamerStatus :: Too many gamers! Given: %d, Max: %d", nGamers, MAX_GAMER_STATUS);
		return false;
	}

	sm_GetGamerStatusTask = rage_new GetGamerStatusTask();
	sm_GetGamerStatusTask->m_Status.Reset();
	sm_GetGamerStatusTask->m_nGamers = nGamers;

	for(unsigned i = 0; i < nGamers; i++)
	{
		sm_GetGamerStatusTask->m_hGamers[i] = pGamers[i];
		sm_GetGamerStatusTask->m_GamerStates[i] = eGamerState_Invalid;
	}

	// call into the presence function
	bool bSuccess = rlPresenceQuery::GetGamerState(localGamerIndex, 
												 sm_GetGamerStatusTask->m_hGamers, 
												 sm_GetGamerStatusTask->m_nGamers, 
												 sm_GetGamerStatusTask->m_GamerStates, 
												 &sm_GetGamerStatusTask->m_Status);

	return bSuccess; 
}

bool CLiveManager::QueueGamerForStatus(const rlGamerHandle& hGamer)
{
	// create a new queue for script to add gamers to
	if(!sm_GetGamerStateQueue)
		sm_GetGamerStateQueue = rage_new GetGamerStatusQueue(); 

	if(!gnetVerify(sm_GetGamerStateQueue->m_nGamers < MAX_GAMER_STATUS))
	{
		gnetError("QueueGamerForStatus :: No space in queue! Max: %d", MAX_GAMER_STATUS);
		return false;
	}
	
	sm_GetGamerStateQueue->m_GamerQueue[sm_GetGamerStateQueue->m_nGamers++] = hGamer;
	return true;
}

bool CLiveManager::GetGamerStatusFromQueue()
{
	// check we have a queue
	if(!gnetVerify(sm_GetGamerStateQueue))
	{
		gnetError("GetGamerStatusFromQueue :: No queue!");
		return false;
	}

	// check we have some gamers in the queue
	if(!gnetVerify(sm_GetGamerStateQueue->m_nGamers > 0))
	{
		gnetError("GetGamerStatusFromQueue :: No gamers in queue!");
		return false;
	}

	// call into standard function
	bool bSuccess = GetGamerStatus(sm_GetGamerStateQueue->m_GamerQueue, sm_GetGamerStateQueue->m_nGamers);

	// delete the queue
	delete sm_GetGamerStateQueue;
	sm_GetGamerStateQueue = NULL;

	// indicate result
	return bSuccess;
}

bool CLiveManager::IsGettingGamerStatus()
{
	return sm_GetGamerStatusTask && sm_GetGamerStatusTask->m_Status.Pending();
}

bool CLiveManager::DidGetGamerStatusSucceed()
{
	// verify that we don't have an active task
	if(!gnetVerify(sm_GetGamerStatusTask))
	{
		gnetError("DidGetGamerStatusSucceed :: Task does not exist!");
		return false;
	}

	return sm_GetGamerStatusTask->m_Status.Succeeded();
}

unsigned CLiveManager::GetNumGamerStatusGamers()
{
	// verify that we don't have an active task
	if(!gnetVerify(sm_GetGamerStatusTask))
	{
		gnetError("GetNumGamerStatusGamers :: Task does not exist!");
		return 0;
	}

	return sm_GetGamerStatusTask->m_nGamers;
}

eGamerState* CLiveManager::GetGamerStatusStates()
{
	// verify that we don't have an active task
	if(!gnetVerify(sm_GetGamerStatusTask))
	{
		gnetError("GetGamerStatusStates :: Task does not exist!");
		return NULL;
	}

	return sm_GetGamerStatusTask->m_GamerStates;
}

eGamerState CLiveManager::GetGamerStatusState(int nIndex)
{
	// verify that we don't have an active task
	if(!gnetVerify(sm_GetGamerStatusTask))
	{
		gnetError("GetGamerStatusStates :: Task does not exist!");
		return eGamerState_Invalid;
	}

	// verify that we don't have an active task
	if(!gnetVerify(nIndex >= 0 && nIndex < sm_GetGamerStatusTask->m_nGamers))
	{
		gnetError("GetGamerStatusStates :: Invalid gamer status index of %d!", nIndex);
		return eGamerState_Invalid;
	}

	return sm_GetGamerStatusTask->m_GamerStates[nIndex];
}

rlGamerHandle* CLiveManager::GetGamerStatusHandles()
{
	// verify that we don't have an active task
	if(!gnetVerify(sm_GetGamerStatusTask))
	{
		gnetError("GetGamerStatusHandles :: Task does not exist!");
		return NULL;
	}

	return sm_GetGamerStatusTask->m_hGamers;
}

bool CLiveManager::GetGamerStatusHandle(int nIndex, rlGamerHandle* phGamer)
{
	phGamer->Clear();

	// verify that we don't have an active task
	if(!gnetVerify(sm_GetGamerStatusTask))
	{
		gnetError("GetGamerStatusStates :: Task does not exist!");
		return false;
	}

	// verify that we don't have an active task
	if(!gnetVerify(nIndex >= 0 && nIndex < sm_GetGamerStatusTask->m_nGamers))
	{
		gnetError("GetGamerStatusStates :: Invalid gamer status index of %d!", nIndex);
		return false;
	}

	*phGamer = sm_GetGamerStatusTask->m_hGamers[nIndex];
	return true;
}

void CLiveManager::ClearGetGamerStatus()
{
	gnetDebug1("ClearGetGamerStatus");

	if(sm_GetGamerStatusTask)
	{
        if(sm_GetGamerStatusTask->m_Status.Pending())
        {
            gnetDebug1("ClearGetGamerStatus :: Cancelling in-flight task");
            netTask::Cancel(&sm_GetGamerStatusTask->m_Status);
        }

		delete sm_GetGamerStatusTask;
		sm_GetGamerStatusTask = NULL;
	}
}

//private:

void
CLiveManager::SetMainGamerInfo(const rlGamerInfo& gamerInfo)
{
	gnetAssert(gamerInfo.IsValid() || !gamerInfo.IsSignedIn());

	bool bProcessStateChange = (rlPresence::GetActingGamerInfo().GetGamerId() != gamerInfo.GetGamerId());
	bool bProcessOnlineChange = bProcessStateChange | (rlPresence::GetActingGamerInfo().IsOnline() != gamerInfo.IsOnline());

	gnetDebug1("SetMainGamerInfo :: Setting to: \"%s\", Handle: %s, SignedIn: %s, Online: %s, StateChange: %s, OnlineChange: %s",
		gamerInfo.IsSignedIn() ? gamerInfo.GetName() : "None",
		gamerInfo.IsSignedIn() ? NetworkUtils::LogGamerHandle(gamerInfo.GetGamerHandle()) : "None",
		gamerInfo.IsSignedIn() ? "True" : "False",
		gamerInfo.IsOnline() ? "True" : "False",
		bProcessStateChange ? "True" : "False",
		bProcessOnlineChange ? "True" : "False");

	if(bProcessStateChange)
    {
        sm_AchMgr.m_NeedToRefreshAchievements = gamerInfo.IsSignedIn();
        sm_AchMgr.m_NumAchRead = 0;
	}

	if (bProcessOnlineChange)
	{
		sm_FriendsInMatch = 0;
		sm_InviteMgr.ClearInvite(gamerInfo.IsSignedIn() ? InviteMgr::CF_Default : InviteMgr::CF_Force);
	}

	// A new gamer info, or the gamer has gone offline. Both require rlFriendsManager deactivation
	if (bProcessStateChange || (bProcessOnlineChange && !gamerInfo.IsOnline()))
	{
		rlFriendsManager::Deactivate();
	}

	// A new gamer info or the current gamer info has gone online
	if ((bProcessStateChange || bProcessOnlineChange))
	{
#if RSG_XDK
		sm_OverallReputationIsBad = false;
#endif

#if RSG_XBOX
		sm_bHasAvoidList = false;
		sm_AvoidList.m_Players.Reset();
#endif

		if(gamerInfo.IsOnline())
		{
			rlFriendsManager::Activate(gamerInfo.GetLocalIndex());

#if RSG_XDK
			sm_OverallReputationIsBadNeedsRefreshed = true; 
			sm_bAvoidListNeedsRefreshed = true; 
#endif
#if RSG_XBOX
#endif
		}
	}

	// set the new info
	int newGamerIndex = RL_INVALID_GAMER_INDEX;
	if(RL_IS_VALID_LOCAL_GAMER_INDEX(gamerInfo.GetLocalIndex()) && gamerInfo.IsSignedIn())
	{
		rlPresence::SetActingGamerInfo(gamerInfo);
		newGamerIndex = gamerInfo.GetLocalIndex();

#if RSG_SCE
		g_rlNp.GetNpAuth().RegisterPlusCallbacks();
#endif
	}
	else
	{
		rlPresence::ClearActingGamerInfo();
		CLiveManager::ClearMainGamerInfo();

#if RSG_SCE
		g_rlNp.GetNpAuth().UnregisterPlusCallbacks();
#endif
	}

	// set the acting user index
	rlPresence::SetActingUserIndex(gamerInfo.GetLocalIndex());

	if(gamerInfo.IsOnline() && !PARAM_nonetwork.Get())
	{
#if RSG_NP || RSG_XBOX
		if(!sm_RichPresenceMgr.IsInitialized())
		{
			richPresenceMgr::ActivatePresenceCallback pCb(&rlPresence::SetStatusString);
#if RSG_DURANGO
			richPresenceMgr::ActivatePresenceContextCallback cCB(&events_durango::WritePresenceContextEvent);
			sm_RichPresenceMgr.Init(pCb, cCB, CLiveManager::GetRlineAllocator());
#else
			sm_RichPresenceMgr.Init(pCb, CLiveManager::GetRlineAllocator());
#endif
		}
#endif // RSG_NP || RSG_XBOX
	}
	else 
	{
		// no longer online, bail out of RPM
		if(sm_RichPresenceMgr.IsInitialized())
			sm_RichPresenceMgr.Shutdown(SHUTDOWN_CORE);
	}

	// apply info to other systems
	sm_scGamerDataMgr.SetActiveGamerIndex(newGamerIndex);
    CPlayerInfo* pPlayerInfo = CGameWorld::GetMainPlayerInfoSafe();
    if(pPlayerInfo)
	{
		pPlayerInfo->m_GamerInfo = rlPresence::GetActingGamerInfo();
	}

	if(NetworkInterface::IsLocalPlayerOnline())
	{
		sm_ProfileStatsMgr.Init();
	}

#if RSG_XBOX
	// The presence mgr gets shut down when the active user signs out so check for that. He may still be flagged as online here.
	for(int i = 0; i < RL_MAX_LOCAL_GAMERS && NetworkInterface::GetRichPresenceMgr().IsInitialized(); i++)
	{
		rlGamerInfo gi;
		if(RL_IS_VALID_LOCAL_GAMER_INDEX(i) && i != newGamerIndex && rlPresence::IsOnline(i) && rlPresence::GetGamerInfo(i, &gi))
		{
			NetworkInterface::GetRichPresenceMgr().SetPresenceStatus(i, player_schema::PRESENCE_PRES_10, NULL, NULL, 0);
		}
	}
#endif 

	// voice manages state based on the current player
	if((bProcessStateChange || bProcessOnlineChange))
	{
		CNetwork::GetVoice().OnMainGamerAssigned();
	}

#if RSG_DURANGO
	CProfileSettings::GetInstance().UpdateProfileSettings( false );
#endif // RSG_DURANGO

	SCE_ONLY(CGenericGameStorage::UpdateUserId(newGamerIndex);)

	SCE_ONLY(g_rlNp.OnNpActingUserChange(newGamerIndex);)

	// Sets the game version 
	static bool s_presenceVersionSet = false;
	if(!s_presenceVersionSet && NetworkInterface::IsLocalPlayerOnline() && NetworkInterface::HasValidRosCredentials())
	{
		gnetAssert(rlPresence::SetStringAttribute(newGamerIndex, "buildver", CDebug::GetVersionNumber()));
		s_presenceVersionSet=true;
	}

    if(gamerInfo.IsOnline())
    {
        sm_bSubscriptionTelemetryPending = true;
        CheckAndSendSubscriptionTelemetry();
    }
}

void
CLiveManager::ClearMainGamerInfo()
{
	gnetDebug1("ClearMainGamerInfo");

#if RSG_XBOX
	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if (RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
	{
		rlPrivileges::RefreshPrivileges(localGamerIndex);
	}
#endif

	sm_AchMgr.m_NeedToRefreshAchievements = false;
	sm_AchMgr.m_NumAchRead = 0;
	sm_AchMgr.ClearAchievements();

	rlFriendsManager::Deactivate();
	sm_FriendsPage.ClearFriendsArray();
	sm_FriendsReadBuf.m_StartIndex = 0;
	sm_FriendsReadBuf.ClearFriendsArray();
	sm_FriendsInMatch = 0;

	rlPresence::ClearActingGamerInfo();
    sm_scGamerDataMgr.SetActiveGamerIndex(-1);

	sm_InviteMgr.ClearAll();
	sm_PartyInviteMgr.ClearAll();
}

void 
CLiveManager::RefreshPrivileges()
{
    gnetDebug1("RefreshPrivileges");

    for(int i = 0; i < RL_MAX_LOCAL_GAMERS; i++)
    {
		// permission valid unless PRESENCE_EVENT_ONLINE_PERMISSIONS_INVALID event fired
		sm_bPermissionsValid[i] = true;

		// online checks
        if(rlPresence::IsOnline(i))
        {
            sm_bHasOnlinePrivileges[i] = rlGamerInfo::HasMultiplayerPrivileges(i);
			sm_bHasUserContentPrivileges[i] = rlGamerInfo::HasUserContentPrivileges(i);
			sm_bHasVoiceCommunicationPrivileges[i] = rlGamerInfo::HasChatPrivileges(i);
			sm_bHasTextCommunicationPrivileges[i] = rlGamerInfo::HasTextPrivileges(i);
        }
        else
        {
            sm_bHasOnlinePrivileges[i] = false;
			sm_bHasUserContentPrivileges[i] = false;
			sm_bHasVoiceCommunicationPrivileges[i] = false;
			sm_bHasTextCommunicationPrivileges[i] = false;
        }

		// signed in checks
		if(rlPresence::IsSignedIn(i))
		{
#if RSG_ORBIS
			// only the active player is considered on PS4
			sm_bConsiderForPrivileges[i] = ((i == NetworkInterface::GetLocalGamerIndex()) || (i == CControlMgr::GetMainPlayerIndex()));
#else
			sm_bConsiderForPrivileges[i] = true;
#endif
			sm_bHasSocialNetSharePriv[i] = rlGamerInfo::HasSocialNetworkingPrivileges(i);
			sm_bIsRestrictedByAge[i] = rlGamerInfo::IsRestrictedByAge(i);

#if !__NO_OUTPUT
			char szNameBuf[RL_MAX_NAME_BUF_SIZE];
			rlPresence::GetName(i, szNameBuf);
#endif 
			// log out each online user
			gnetDebug1("RefreshPrivileges :: [%d] Gamer: %s, Considered: %s, Online: %s, Multiplayer: %s, Content: %s, Voice: %s, Text: %s, Age: %d, AgeRestriction: %s, SocialNetwork: %s", 
						i,
						szNameBuf,
						sm_bConsiderForPrivileges[i] ? "True" : "False",
						rlPresence::IsOnline(i) ? "True" : "False",
						sm_bHasOnlinePrivileges[i] ? "True" : "False",
						sm_bHasUserContentPrivileges[i] ? "True" : "False",
						sm_bHasVoiceCommunicationPrivileges[i] ? "True" : "False",
						sm_bHasTextCommunicationPrivileges[i] ? "True" : "False",
						rlPresence::GetAge(i),
						sm_bIsRestrictedByAge[i] ? "True" : "False",
						sm_bHasSocialNetSharePriv[i] ? "True" : "False");
		}
		else
		{
			sm_bConsiderForPrivileges[i] = false;
			sm_bHasSocialNetSharePriv[i] = false;
			sm_bIsRestrictedByAge[i] = false;
		}
    }
}

#if NET_ENABLE_MEMBERSHIP_FUNCTIONALITY
void CLiveManager::SetHasShownMembershipWelcome()
{
	if(!sm_HasShownMembershipWelcome)
	{
		gnetDebug1("SetHasShownMembershipWelcome");
		sm_HasShownMembershipWelcome = true;
	}
}

void CLiveManager::SetSuppressMembershipForScript(const bool suppress)
{
	if(sm_SuppressMembershipForScript != suppress)
	{
		// if we were suppressing, fire any membership events we cached
		if(sm_SuppressMembershipForScript)
		{
			if(sm_HasCachedMembershipReceivedEvent)
			{
				gnetDebug1("SetSuppressMembershipForScript :: Generating cached rlScMembershipStatusReceivedEvent");
				OnMembershipEvent(&sm_MembershipReceivedEvent);
				sm_HasCachedMembershipReceivedEvent = false;
			}

			if(sm_HasCachedMembershipChangedEvent)
			{
				gnetDebug1("SetSuppressMembershipForScript :: Generating cached rlScMembershipStatusChangedEvent");
				OnMembershipEvent(&sm_MembershipChangedEvent);
				sm_HasCachedMembershipChangedEvent = false;
			}
		}

		gnetDebug1("SetSuppressMembershipForScript :: %s -> %s", sm_SuppressMembershipForScript ? "True" : "False", suppress ? "True" : "False");
		sm_SuppressMembershipForScript = suppress;
	}
}
#endif

void CLiveManager::CheckAndSendSubscriptionTelemetry()
{
#if RSG_ORBIS || RSG_DURANGO
    if(sm_bSubscriptionTelemetryPending)
    {
        if(!IsOnline())
        {
            gnetDebug1("CheckAndSendSubscriptionTelemetry :: Not online");
            return;
        }

        if(!IsOnlineRos())
        {
            gnetDebug1("CheckAndSendSubscriptionTelemetry :: No Rockstar credentials");
            return;
        }

        if(!sm_bInitialisedSession)
        {
            gnetDebug1("CheckAndSendSubscriptionTelemetry :: Not initialised session");
            return;
        }

        gnetDebug1("CheckAndSendSubscriptionTelemetry :: Sending");
        
        bool bHasPlatformSubscription = 
#if RSG_ORBIS
            NetworkInterface::HasRawPlayStationPlusPrivileges();
#else
            CheckOnlinePrivileges();
#endif

        // send our subscription metric here - we don't have a subscription on PC so no need to send
        MetricMultiplayerSubscription m(bHasPlatformSubscription);
        APPEND_METRIC(m);

        sm_bSubscriptionTelemetryPending = false;
    }
#endif
}

void CLiveManager::CheckAndSendMembershipTelemetry()
{
#if NET_ENABLE_MEMBERSHIP_METRICS
	if(!sm_bMembershipTelemetryPending)
		return;

#if NET_ENABLE_MEMBERSHIP_FUNCTIONALITY
	// wait until the status is known
	const rlScMembershipTelemetryState membershipState = rlScMembership::GetMembershipTelemetryState(NetworkInterface::GetLocalGamerIndex());
	if(membershipState == rlScMembershipTelemetryState::State_Unknown_NotRequested)
		return;
#else
	// without the functionality enabled, just wait for credentials
	if(!IsOnlineRos())
		return;
#endif

	// send our membership status here (follow up: send an update if this expires)
	MetricScMembership m;
	APPEND_METRIC(m);

	sm_bMembershipTelemetryPending = false;
#endif
}

void CLiveManager::UpdateMembership()
{
	CheckAndSendMembershipTelemetry();

#if NET_ENABLE_MEMBERSHIP_FUNCTIONALITY && RSG_SCARLETT
	// check for any changes to commerce that would force a membership refresh
	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
	{
		// this doesn't matter if we already have membership
		const bool hasMembership = rlScMembership::HasMembership(localGamerIndex);
		if(!hasMembership)
		{
			cCommerceConsumableManager* consumableMgr = sm_Commerce.GetConsumableManager();
			if(consumableMgr != nullptr)
			{
				// we should really have a commerce event system but this will do for now (mirrors an existing example in StoreMainScreen.cpp)
				static bool s_WasOwnershipDataPending = false;
				const bool isOwnershipDataPending = consumableMgr->IsOwnershipDataPending();
				const bool isOwnershipDataPopulated = consumableMgr->IsOwnershipDataPopulated();

				// if we we pending, now aren't and the operation succeeded (i.e. we have data)...
				if(s_WasOwnershipDataPending && !isOwnershipDataPending && isOwnershipDataPopulated)
				{
					if(consumableMgr->IsConsumableKnown(rlScMembership::GetSubscriptionId()))
					{
						gnetDebug1("UpdateMembership :: New Ownership Data Indicates Membership -> Requesting Status");
						rlScMembership::RequestMembershipStatus(localGamerIndex);
					}
				}
				s_WasOwnershipDataPending = isOwnershipDataPending;
			}
		}
	}
#endif
}

#if RSG_SCE
// app launch flags
enum eLaunchFlags
{
	LAUNCH_STRAIGHT_INTO_FREEMODE = BIT0,
	LAUNCH_FROM_LIVE_AREA = BIT1,
	LAUNCH_DIRECT_TO_CONTENT = BIT2,
};
#endif

void
CLiveManager::OnPresenceEvent(const rlPresenceEvent* pEvent)
{
	// log event
	gnetDebug1("OnPresenceEvent :: %s received for %s", pEvent->GetAutoIdNameFromId(pEvent->GetId()), pEvent->m_GamerInfo.IsValid() ? pEvent->m_GamerInfo.GetName() : "CONSOLE");

	// loading screens is interested in these
	NPROFILE(CLoadingScreens::OnPresenceEvent(pEvent));

	// event specific processing
	switch(pEvent->GetId())
	{
	case PRESENCE_EVENT_SIGNIN_STATUS_CHANGED:
		{
			const rlPresenceEventSigninStatusChanged* pThisEvent = pEvent->m_SigninStatusChanged;

			bool bActionProfileChange = false;
			bool bBailFromMulti = false;
			int nControllerIndex = CControlMgr::GetMainPlayerIndex();
			eBailReason bailReason = eBailReason::BAIL_PROFILE_CHANGE;
			int bailContext = BAIL_CTX_PROFILE_CHANGE_SIGN_OUT;

#if RSG_PC
			// PC only supports player index 0 - regardless of which controller is used
			nControllerIndex = 0;
#endif

			// stash these before we apply the new settings
			bool bWasSignedIn = IsSignedIn();
			bool bWasOnline = IsOnline();
			bool bIsActiveGamer = nControllerIndex == pThisEvent->m_GamerInfo.GetLocalIndex();

			// only process for the active controller
			if(bIsActiveGamer)
			{
				gnetDebug1("OnPresenceEvent :: Signin status changed for local player.");

				if(pThisEvent->SignedIn())
				{
					gnetDebug1("OnPresenceEvent :: Signed In");

					// Trigger stats cleanup
					NPROFILE(StatsInterface::SignedIn());

					// we already think this profile is signed in - the profile has changed on this controller
					// could happen if the gamer signed out and back in again without a chance to process events
					// or signed in over the top (can be done through Xbox sign-in UI)
					rlGamerInfo* pActingInfo = rlPresence::GetActingGamerInfoPtr();
					if(pActingInfo && (NetworkInterface::GetLocalGamerIndex() == pThisEvent->m_GamerInfo.GetLocalIndex()))
					{
						// different profile 
						if(pActingInfo->GetGamerId() != pThisEvent->m_GamerInfo.GetGamerId())
						{
							gnetDebug1("OnPresenceEvent :: Signed In, different Profile");
							bActionProfileChange = true;
						}
						// online state changed - should fire via SignedOffline
						else if(NetworkInterface::IsLocalPlayerOnline() && !pThisEvent->m_GamerInfo.IsOnline())
						{
							gnetDebug1("OnPresenceEvent :: Signed In, in MP so bailing.");
							bBailFromMulti = true; 
						}
					}
				}
				else if(pThisEvent->SignedOnline())
				{
					gnetDebug1("OnPresenceEvent :: Signed Online");

                    if(sm_bOfflineInvitePending)
                    {
                        gnetDebug1("OnPresenceEvent :: Offline Invite Pending. Clearing.");
                        sm_bOfflineInvitePending = false;
                    }

#if RSG_XDK
					// refresh party (if this came from a suspend)
					g_rlXbl.GetPartyManager()->FlagRefreshParty();
#endif

					// Trigger stats cleanup
					NPROFILE(StatsInterface::SignedIn();)
					NPROFILE(CNetwork::GetVoice().OnSignedOnline();)
					NPROFILE(CNetworkTelemetry::SignedOnline();)
					NPROFILE(Tunables::GetInstance().OnSignedOnline();)
				}
				else if(pThisEvent->SignedOffline())
				{
					gnetDebug1("OnPresenceEvent :: Signed Offline");

					// check that we'd matched this profile up with our main info
					gnetAssertf(NetworkInterface::GetLocalGamerIndex() == nControllerIndex, "OnPresenceEvent :: Controller index (%d) does not match local index (%d)!", NetworkInterface::GetLocalGamerIndex(), nControllerIndex);

					// restart only if we're in a network session
					bBailFromMulti = (!pThisEvent->m_GamerInfo.IsOnline());

					// check if this was due to losing connection
					if(pThisEvent->WasConnectionLost())
					{
						gnetDebug1("OnPresenceEvent :: Due to connection lost");
						bailReason = eBailReason::BAIL_PROFILE_CHANGE;
						bailContext = eBailContext::BAIL_CTX_PROFILE_CHANGE_HARDWARE_OFFLINE;
					}
					
                    // kill an active UGC query
                    if(UserContentManager::GetInstance().IsQueryPending())
                        UserContentManager::GetInstance().CancelQuery();

					NPROFILE(CNetworkTelemetry::SignedOut();)
					NPROFILE(CStatsMgr::SignedOffline();)
					NPROFILE(cStoreScreenMgr::OnSignOffline();)
					NPROFILE(sm_NetworkClanDataMgr.OnSignOffline();)
					NPROFILE(CGameSessionStateMachine::OnSignedOffline();)
				}
				else if(pThisEvent->SignedOut())
				{
					gnetDebug1("OnPresenceEvent :: Signed Out (Duplicate Login: %s)", pThisEvent->WasDuplicateLogin() ? "TRUE" : "FALSE");

					// check that we'd matched this profile up with our main info
					gnetAssertf(NetworkInterface::GetLocalGamerIndex() == nControllerIndex, "OnPresenceEvent :: Controller index (%d) does not match local index (%d)!", NetworkInterface::GetLocalGamerIndex(), nControllerIndex);

					// always kill network sessions
					bBailFromMulti = true;

                    // kill this - irrelevant now
                    sm_bOfflineInvitePending = false;

#if !RSG_XBOX
					// Trigger stats cleanup
					StatsInterface::SignedOut();
#endif

					NPROFILE(CPauseMenu::Close();)
					NPROFILE(CNetworkTelemetry::SignedOut();)

					// Cancel any in-flight friend status tasks
					if (sm_FriendStatus.Pending())
					{
#if RSG_XBOX
						rlGetTaskManager()->CancelTask(&sm_FriendStatus);
#else
						netTask::Cancel(&sm_FriendStatus);
#endif
					}

#define CHECK_CONDITION(condition, parameter, loggingString) \
	if(parameter && (condition)) \
	{ \
		gnetDebug1(loggingString); \
		parameter = false; \
	}

#if RSG_XBOX
					bool delaySignOutProcessingForSuspend = true;

					// if we haven't resumed recently, process the sign out
					CHECK_CONDITION(!HasRecentResume(), delaySignOutProcessingForSuspend, "OnPresenceEvent :: Signed Out :: Processing - No recent resume");

					// if this is multiplayer, process the sign out
					CHECK_CONDITION(CLiveManager::ShouldBailFromMultiplayer(), delaySignOutProcessingForSuspend, "OnPresenceEvent :: Signed Out :: Processing - In Multiplayer");

					// if this is the video editor, process the sign out
					CHECK_CONDITION(CVideoEditorUi::IsActive(), delaySignOutProcessingForSuspend, "OnPresenceEvent :: Signed Out :: Processing - In Video Editor");
	
					// if the delay flag wasn't reset, process this on a counter
					if(delaySignOutProcessingForSuspend)
					{
						// store the signing out handle to check when we sign back in the need to start a new game
						gnetDebug1("OnPresenceEvent :: Signed Out :: Postponing - Handle: %s", pThisEvent->m_GamerInfo.GetGamerHandle().ToString());
						sm_SignedOutHandle = pThisEvent->m_GamerInfo.GetGamerHandle();
						sm_ProfileChangeActionCounter = GetProfileChangeActionDelayMs();
					}
					else
#endif
					{
						bool shouldActionProfileChange = true; 
#if !RSG_GDK
						// moved from when we check for postpone - though this should perhaps just be removed
						CHECK_CONDITION(XBOX_ONLY(!sm_SignedInIntoNewProfileAfterSuspendMode &&) (CLoadingScreens::LoadingScreensPendingShutdown() || CLoadingScreens::AreActive()), shouldActionProfileChange, "OnPresenceEvent :: Signed Out :: Skipping because loading screens are active!");
#endif

						// if there are no blockers, action the change
						if(shouldActionProfileChange)
						{
							gnetDebug1("OnPresenceEvent :: Signed Out :: Processing Profile Change");

							// assume we action a profile change
							bActionProfileChange = true;

							// clear reference since we are already processing the profile change
							XBOX_ONLY(sm_SignedOutHandle.Clear());
						}
					}

#if defined( GTA_REPLAY ) && GTA_REPLAY
					NPROFILE(CVideoEditorUi::OnSignOut( );)
					NPROFILE(CReplayMgr::OnSignOut( );)
#endif
					
#if RSG_PC
					NPROFILE(CTextInputBox::OnSignOut();)
#endif // RSG_PC

					NPROFILE(rlPrivileges::OnSignOut(pThisEvent->m_GamerInfo.GetLocalIndex());)
					NPROFILE(cStoreScreenMgr::OnSignOut();)
					NPROFILE(CNetwork::GetVoice().OnSignOut();)
					NPROFILE(Tunables::GetInstance().OnSignOut();)
					NPROFILE(CGameSessionStateMachine::OnSignOut();)

#if RSG_XBOX
					NPROFILE(g_rlXbl.GetProfileManager()->OnSignOut());
#endif

					// reset tracking
					sm_bHasCredentialsResult[pThisEvent->m_GamerInfo.GetLocalIndex()] = false;
				}

#if RSG_XBOX
				// if we don't have any main gamer then we may need to action a profile on Xbox
				// depending if this is a complete new profile or it is the same when the console went into standby.
				if(!rlPresence::GetActingGamerInfo().IsValid())
				{
					if(pThisEvent->m_GamerInfo.IsValid())
					{
						if(sm_SignedOutHandle.IsValid() && (sm_SignedOutHandle != pThisEvent->m_GamerInfo.GetGamerHandle()))
						{
							gnetDebug1("OnPresenceEvent :: SignedIn :: Different Handle from Previous, Now: %s, Was: %s", pThisEvent->m_GamerInfo.GetGamerHandle().ToString(), sm_SignedOutHandle.ToString());
							
							// different profile than we cached, sign out
							bActionProfileChange = true;

							// whether we set this or not
							bool flagAsNewProfileDuringSuspend = true; 

							// we don't want to do this on the IIS in gen9 as this would begin a delayed start new game flow 
							// as soon as the game loaded in if the user changed on this screen
							SCARLETT_ONLY(CHECK_CONDITION(CInitialInteractiveScreen::IsActive(), flagAsNewProfileDuringSuspend, "OnPresenceEvent :: On IIS, so not flagging as new profile during suspend"));
							
							// this is used by external systems to know that the profile was switched during a PLM event
							if(flagAsNewProfileDuringSuspend)						
								sm_SignedInIntoNewProfileAfterSuspendMode = true;

							// trigger stats cleanup
							StatsInterface::SignedOut();
						}
						else
						{
							gnetDebug1("OnPresenceEvent :: SignedIn :: Clearing Cached SignOut Data");
							sm_SignedOutHandle.Clear();
							sm_SignedInIntoNewProfileAfterSuspendMode = m_SignedInIntoNewProfileAfterSuspendMode_Default;
						}
					}
				}
#endif // RSG_XBOX

				// update main info
				SetMainGamerInfo(pThisEvent->m_GamerInfo);

				// process some systems after the main gamer has been set
				if(pThisEvent->SignedIn())
				{
					
				}
				else if(pThisEvent->SignedOnline())
				{
					NPROFILE(CloudManager::GetInstance().OnSignedOnline();)
				}
				else if(pThisEvent->SignedOffline())
				{
					NPROFILE(CloudManager::GetInstance().OnSignedOffline();)
				}
				else if(pThisEvent->SignedOut())
				{
					NPROFILE(CloudManager::GetInstance().OnSignOut();) 
				}
			}
			
			bool shouldTriggerSignInChangeEvent = true;

#if RSG_PROSPERO || RSG_SCARLETT
			// script now match Gen8 for the sign out event so this is safe to fire in all cases (previously it would always trigger an IIS switch)
			// we need to eventually move sign out actioning to the CEventNetworkProfileChangeActioned event instead so that we 
			// correctly account for PLM changes where we briefly sign out and then back in again
			//CHECK_CONDITION(!bActionProfileChange, shouldTriggerSignInChangeEvent, "OnPresenceEvent :: Not triggering sign in event for non-profile change");
#endif

			if(shouldTriggerSignInChangeEvent)
			{
				// trigger network event
				GetEventScriptNetworkGroup()->Add(CEventNetworkSignInStateChanged(pThisEvent->m_GamerInfo.GetLocalIndex(),
					bIsActiveGamer,
					bIsActiveGamer ? bWasSignedIn : pThisEvent->SignedOut(),
					bIsActiveGamer ? bWasOnline : pThisEvent->SignedOffline(),
					bIsActiveGamer ? IsSignedIn() : rlPresence::IsSignedIn(pThisEvent->m_GamerInfo.GetLocalIndex()),
					bIsActiveGamer ? IsOnline() : rlPresence::IsOnline(pThisEvent->m_GamerInfo.GetLocalIndex()),
					pThisEvent->WasDuplicateLogin()));
			}

#if RSG_XBOX
			if(pThisEvent->SignedOnline() && NetworkInterface::GetRichPresenceMgr().IsInitialized())
			{
				for(int i = 0; i < RL_MAX_LOCAL_GAMERS; i++)
				{
					rlGamerInfo gi;
					if(RL_IS_VALID_LOCAL_GAMER_INDEX(i) && i != NetworkInterface::GetLocalGamerIndex() && rlPresence::IsOnline(i) && rlPresence::GetGamerInfo(i, &gi))
					{
						NetworkInterface::GetRichPresenceMgr().SetPresenceStatus(i, player_schema::PRESENCE_PRES_10, NULL, NULL, 0);
					}
				}
			}
#endif // RSG_DURANGO

			// restart session?
			if(bBailFromMulti)
			{
				// Check if the profile change was due to a loss of network access.
				if(!NetworkInterface::HasNetworkAccess())
				{
					gnetDebug1("OnPresenceEvent :: No network access");
					bailReason = BAIL_PROFILE_CHANGE;
					bailContext = BAIL_CTX_PROFILE_CHANGE_HARDWARE_OFFLINE;
				}

				bailReason = sm_bResumeActionPending ? BAIL_SYSTEM_SUSPENDED : bailReason;
				bailContext = sm_bResumeActionPending ? BAIL_CTX_SUSPENDED_SIGN_OUT : bailContext;

				CLiveManager::BailOnMultiplayer(bailReason, bailContext);
                CLiveManager::BailOnVoiceSession();
				NET_SHOP_ONLY(NPROFILE(GameTransactionSessionMgr::Get().ShutdownManager()))
			}
				
			// if we have flagged to action a profile change, check if we can action it now...
			if(bActionProfileChange)
			{
				if(CLiveManager::CanActionProfileChange(OUTPUT_ONLY(true)))
				{
					gnetDebug1("OnPresenceEvent :: Processing ActionProfileChange");
					ActionProfileChange(); 
				}
				else
				{
					gnetDebug1("OnPresenceEvent :: Delaying ActionProfileChange");
					sm_PostponeProfileChange = true;
				}
			}

			// refresh after sign in change has been processed
			NPROFILE(RefreshPrivileges();)
		}
		break; 

	case PRESENCE_EVENT_SC_MESSAGE:
		{
			const rlPresenceEventScMessage* pThisEvent = pEvent->m_ScMessage;
			const rlScPresenceMessage& message = pThisEvent->m_Message;
			const rlScPresenceMessageSender& sender = pThisEvent->m_Sender;
			if(message.IsA<rlScPresenceMessageMpInvite>())
			{
				rlScPresenceMessageMpInvite msg;
				if(msg.Import(message, sender))
				{
					if((rlPresence::GetActingGamerInfo().GetGamerId() == pThisEvent->m_GamerInfo.GetGamerId()) && CheckOnlinePrivileges())
					{
						gnetDebug1("OnPresenceEvent :: Received an SC msg invitation from:\"%s\"", msg.m_InviterName);
						sm_InviteMgr.AddUnacceptedInvite(msg.m_SessionInfo, msg.m_InviterGamerHandle, msg.m_InviterName, "");
					}
				}
			}
			else if (message.IsA<rlScPresenceMessageMpPartyInvite>())
			{
				rlScPresenceMessageMpPartyInvite msg;
				if(msg.Import(message, sender))
				{
					gnetDisplay("OnPresenceEvent: received a party invite from %s, salutation=%s", msg.m_InviterName, msg.m_Salutation);
					sm_PartyInviteMgr.AddInvite(msg.m_SessionInfo, msg.m_InviterGamerHandle, msg.m_InviterName, msg.m_Salutation);
				}
			}
			else if (message.IsA<rlScPresenceInvalidateTicket>())
			{
				if(ShouldBailFromMultiplayer() && (rlPresence::GetActingGamerInfo().GetGamerId() == pThisEvent->m_GamerInfo.GetGamerId()))
				{
					gnetDisplay("OnPresenceEvent: received a ticket invalidation. Bailing from multiplayer. Gamer %s", pThisEvent->m_GamerInfo.GetName());
					CLiveManager::BailOnMultiplayer(BAIL_INVALIDATED_ROS_TICKET, BAIL_CTX_NONE);
				}
			}
		}
		break;

	case PRESENCE_EVENT_INVITE_RECEIVED:
		{
			const rlPresenceEventInviteReceived* pThisEvent = pEvent->m_InviteReceived;

			if((rlPresence::GetActingGamerInfo().GetGamerId() == pThisEvent->m_GamerInfo.GetGamerId())
				&& CheckOnlinePrivileges())
			{
				gnetDebug1("OnPresenceEvent :: Received an invitation from:\"%s\"", pThisEvent->m_InviterName);

				sm_InviteMgr.AddUnacceptedInvite(pThisEvent->m_SessionInfo,
												 pThisEvent->m_Inviter,
												 pThisEvent->m_InviterName,
												 pThisEvent->m_Salutation);
			}
		}
		break;

	case PRESENCE_EVENT_PARTY_INVITE_RECEIVED:
		{
			rlPresenceEventPartyInviteReceived* pThisEvent = pEvent->m_PartyInviteReceived;
			gnetDisplay("OnPresenceEvent: received a party invite from %s, salutation=%s", pThisEvent->m_InviterName, pThisEvent->m_Salutation);
			sm_PartyInviteMgr.AddInvite(pThisEvent->m_SessionInfo, pThisEvent->m_Inviter, pThisEvent->m_InviterName, pThisEvent->m_Salutation);
		}
		break;

	case PRESENCE_EVENT_INVITE_ACCEPTED:
		{
			const rlPresenceEventInviteAccepted* pThisEvent = pEvent->m_InviteAccepted;
			sm_InviteMgr.HandleEventPlatformInviteAccepted(pThisEvent);
		}
		break;

#if RSG_XDK
	case PRESENCE_EVENT_GAME_SESSION_READY:
		{
			const rlPresenceEventGameSessionReady* pThisEvent = pEvent->m_GameSessionReady;
			
			// convert origin to invite flag
			int nPartyFlag = 0;
			if(pThisEvent->m_Origin == ORIGIN_FROM_BOOT)
				nPartyFlag = InviteFlags::IF_PlatformPartyBoot;
			else if(pThisEvent->m_Origin == ORIGIN_FROM_INVITE)
				nPartyFlag = InviteFlags::IF_PlatformParty;
			else if(pThisEvent->m_Origin == ORIGIN_FROM_MULTIPLAYER_JOIN)
				nPartyFlag = InviteFlags::IF_PlatformPartyJoin;
			else if(pThisEvent->m_Origin == ORIGIN_FROM_JVP)
				nPartyFlag = InviteFlags::IF_PlatformPartyJvP;
			else
			{
				gnetError("OnPresenceEvent :: Unhandled game session ready origin flag: %d", pThisEvent->m_Origin);
				nPartyFlag = InviteFlags::IF_PlatformPartyJoin;
			}

			sm_InviteMgr.HandleEventGameSessionReady(pThisEvent, InviteFlags::IF_IsPlatform | InviteFlags::IF_AllowPrivate | nPartyFlag);
		}
		break;

	case PRESENCE_EVENT_PARTY_CHANGED:
		if(CNetwork::IsNetworkSessionValid())
			CNetwork::GetNetworkSession().OnPartyChanged();
		break;

	case PRESENCE_EVENT_PARTY_SESSION_INVALID:
		if(CNetwork::IsNetworkSessionValid())
			CNetwork::GetNetworkSession().OnPartySessionInvalid();
		break;
#endif

	case PRESENCE_EVENT_INVITE_ACCEPTED_WHILE_OFFLINE:
        sm_bOfflineInvitePending = true; 
		CLiveManager::ShowSigninUi();
		break;

	case PRESENCE_EVENT_INVITE_UNAVAILABLE:
		{
			const rlPresenceEventInviteUnavailable* pThisEvent = pEvent->m_InviteUnavailable;
			sm_InviteMgr.HandleEventInviteUnavailable(pThisEvent);
		}
		break;

	case PRESENCE_EVENT_JOINED_VIA_PRESENCE:
		{
			const rlPresenceEventJoinedViaPresence* pThisEvent = pEvent->m_JoinedViaPresence;
			sm_InviteMgr.HandleEventPlatformJoinViaPresence(pThisEvent);
		}
		break;

	case PRESENCE_EVENT_ONLINE_PERMISSIONS_INVALID:
		{
			int localGamerIndex = pEvent->m_GamerInfo.GetLocalIndex();
			sm_bPermissionsValid[localGamerIndex] = false;
		}
		break;

	case PRESENCE_EVENT_ONLINE_PERMISSIONS_CHANGED:
		{
			// check if we should allow invites directly into activity sessions
			static const bool DEFAULT_REFRESH_PERMISSIONS = true;
			const bool bRefreshPermissions = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NET_REFRESH_PERMISSIONS_ON_PERMISSIONS_CHANGED", 0xfcbffd5b), DEFAULT_REFRESH_PERMISSIONS);
		
			static const bool DEFAULT_BAIL_IF_ONLINE_PERMISSION_LOST = true;
			const bool bBailForLossOfOnlinePermission = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NET_BAIL_IF_ONLINE_PERMISSION_LOST", 0x87a6225d), DEFAULT_BAIL_IF_ONLINE_PERMISSION_LOST);

            // capture
			const bool bHaveOnlinePrivileges = CheckOnlinePrivileges();

			// refresh privileges
			if(bRefreshPermissions)
			{
				gnetDebug1("OnPresenceEvent :: OnlinePermissionsChanged :: Refreshing permissions");
				RefreshPrivileges();
			}

			// if we had and now don't have...
			if(bBailForLossOfOnlinePermission && bHaveOnlinePrivileges && !CheckOnlinePrivileges())
			{
                gnetDebug1("OnPresenceEvent :: OnlinePermissionsChanged :: Online privilege revoked.");

				// make a call to resolve the privilege 
				XBOX_ONLY(ResolvePlatformPrivilege(NetworkInterface::GetLocalGamerIndex(), rlPrivileges::PrivilegeTypes::PRIVILEGE_MULTIPLAYER_SESSIONS, true));
                    
                // bail from any multiplayer activity
                if(ShouldBailFromMultiplayer())
                {
                    gnetDebug1("OnPresenceEvent :: Bailing from multiplayer.");
                    CLiveManager::BailOnMultiplayer(BAIL_ONLINE_PRIVILEGE_REVOKED, BAIL_CTX_PRIVILEGE_DEFAULT);
                }
			}

			// let script know
			GetEventScriptNetworkGroup()->Add(CEventNetworkOnlinePermissionsUpdated());
		}
		break;

#if RL_NP_SUPPORT_PLAY_TOGETHER
	case PRESENCE_EVENT_PLAY_TOGETHER_HOST:
		{
			const rlPresenceEventPlayTogetherHost* pThisEvent = pEvent->m_PlayTogetherHost;
			
			// registers this group for play together (we'll invite these players when we enter multiplayer)
			CNetwork::GetNetworkSession().RegisterPlayTogetherGroup(pThisEvent->m_Invitees, pThisEvent->m_nInvitees); 

			// check if we should allow invites directly into activity sessions
			static const bool DEFAULT_ALLOW_ACTIVITY = true; 
			bool bAllowActivity = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("PLAY_TOGETHER_ALLOW_ACTIVITY_INVITE", 0xcf0e02d7), DEFAULT_ALLOW_ACTIVITY);

			// if script are already up and running, use an app launched event to push us into multiplayer
			// if we haven't got this far, we handle this as part of the loading process
			if(CNetwork::IsScriptReadyForEvents())
			{
				// if we're already in a session with enough space, we can accommodate the party
				if( CNetwork::GetNetworkSession().IsSessionEstablished() && 
				   (!CNetwork::GetNetworkSession().IsActivitySession() || bAllowActivity) && 
				   (CNetwork::GetNetworkSession().GetNumAvailableSlots() >= pThisEvent->m_nInvitees))
				{
					gnetDebug1("OnPresenceEvent :: PlayTogether - In Session (with %u slots), Sending Invites", CNetwork::GetNetworkSession().GetNumAvailableSlots()); 
					CNetwork::GetNetworkSession().InvitePlayTogetherGroup();
				}
				else
				{
					const unsigned nFlags = LAUNCH_STRAIGHT_INTO_FREEMODE;
					gnetDebug1("OnPresenceEvent :: PlayTogether - Faking App Launch Flags: 0x%08x, Args: %s", nFlags, g_SysService.GetArgs());

					// fire both events while script transition and then we can drop the app launched one
					GetEventScriptNetworkGroup()->Add(CEventNetworkSystemServiceEvent(sysServiceEvent::APP_LAUNCHED, nFlags));
					GetEventScriptNetworkGroup()->Add(CEventNetworkAppLaunched(nFlags));
				}
			}
		}
		break;
#endif

	default:
		break;
	}

	// social club menu is interested in these
	SocialClubMenu::OnPresenceEvent(pEvent);
}

#if ENABLE_SINGLE_PLAYER_CLOUD_SAVES
void CLiveManager::OnPcEvent(rlPc* /*pc*/, const rlPcEvent* evt)
{
	const unsigned evtId = evt->GetId();
	if (evtId == RLPC_EVENT_CLOUD_SAVES_ENABLED_UPDATED)
	{
		rlPcEventCloudSavesEnabledUpdate* msg = (rlPcEventCloudSavesEnabledUpdate*)evt;
		UpdatedSinglePlayerCloudSaveEnabled(msg->m_RosTitleName);
	}
}
#endif

void
CLiveManager::OnRosEvent(const rlRosEvent& rEvent)
{
	// log event
	gnetDebug1("OnRosEvent :: %s received", rEvent.GetAutoIdNameFromId(rEvent.GetId()));

	int nGamerIndex = -1;
	switch(rEvent.GetId())
	{
	case RLROS_EVENT_ONLINE_STATUS_CHANGED:
		{
			const rlRosEventOnlineStatusChanged& statusEvent = static_cast<const rlRosEventOnlineStatusChanged&>(rEvent);
			nGamerIndex = statusEvent.m_LocalGamerIndex;
		}
		break;
	case RLROS_EVENT_LINK_CHANGED:
		{
			const rlRosEventLinkChanged& linkEvent = static_cast<const rlRosEventLinkChanged&>(rEvent);
			nGamerIndex = linkEvent.m_LocalGamerIndex;
		}
		break;
    case RLROS_EVENT_GET_CREDENTIALS_RESULT:
        {
            const rlRosEventGetCredentialsResult& credEvent = static_cast<const rlRosEventGetCredentialsResult&>(rEvent);
            sm_bHasCredentialsResult[credEvent.m_LocalGamerIndex] = true;

            gnetDebug1("\tCredentials Request %s", credEvent.m_Success ? "Successful" : "Failed");

            // inform cloud manager and session systems
			sm_InviteMgr.OnSetCredentialsResult(true, credEvent.m_Success);
            CloudManager::GetInstance().OnCredentialsResult(credEvent.m_Success);
            CNetwork::GetNetworkSession().OnCredentialsResult(credEvent.m_Success);

			// update the environment CRC here (could do this earlier but this is fine and guarantees everything is setup)
			CNetwork::GetAssetVerifier().RefreshEnvironmentCRC();

			if(credEvent.m_Success)
			{
#if NET_ENABLE_MEMBERSHIP_FUNCTIONALITY
				// flag a telemetry refresh
				sm_bMembershipTelemetryPending = true;
				rlScMembership::RequestMembershipStatus(credEvent.m_LocalGamerIndex);
#endif

                CheckAndSendSubscriptionTelemetry();
                UserContentManager::GetInstance().OnGetCredentials(credEvent.m_LocalGamerIndex);

#if __BANK
				NetworkDebug::CredentialsUpdated(credEvent.m_LocalGamerIndex);
#endif
			}

			// check multiplayer privilege
			if(ShouldBailFromMultiplayer())
			{
				// kick player from multiplayer.
				if(!rlRos::HasPrivilege(NetworkInterface::GetLocalGamerIndex(), RLROS_PRIVILEGEID_MULTIPLAYER))
				{
					gnetDebug1("\tBailing. Console ban - RLROS_PRIVILEGEID_MULTIPLAYER is FALSE");
					CLiveManager::BailOnMultiplayer(BAIL_CONSOLE_BAN, BAIL_CTX_NONE);
				}
			}
        }
        break;
	default:
		break;
	}

	// if this is an online status / link change event, update cached values
	if(nGamerIndex >= 0)
	{
		sm_bIsOnlineRos[nGamerIndex] = rlRos::IsOnline(nGamerIndex);
		if(sm_bIsOnlineRos[nGamerIndex])
		{
			rlRosCredentials tCredentials = rlRos::GetCredentials(nGamerIndex);
			sm_bHasValidCredentials[nGamerIndex] = tCredentials.IsValid();
			sm_bHasValidRockstarId[nGamerIndex]  = sm_bHasValidCredentials[nGamerIndex] && (tCredentials.GetRockstarId() != InvalidRockstarId);
		}
		else
		{
			sm_bHasValidCredentials[nGamerIndex] = false;
			sm_bHasValidRockstarId[nGamerIndex]  = false;
		}

		if (nGamerIndex == NetworkInterface::GetLocalGamerIndex() && !sm_bHasValidCredentials[nGamerIndex] && CLiveManager::IsOnline())
		{
			CProfileSettings& settings = CProfileSettings::GetInstance();
			if(settings.AreSettingsValid())
			{
				settings.Set(CProfileSettings::ROS_WENT_DOWN_NOT_NET, 1);
			}
		}

		// trigger network event
		GetEventScriptNetworkGroup()->Add(CEventNetworkRosChanged(sm_bHasValidCredentials[nGamerIndex], sm_bHasValidRockstarId[nGamerIndex]));

		// logging
		gnetDebug1("\tOnRosEvent :: %s Credentials. %s Rockstar ID.", sm_bHasValidCredentials[nGamerIndex] ? "Valid" : "Invalid", sm_bHasValidRockstarId[nGamerIndex] ? "Valid" : "Invalid");
	}

	SocialClubMenu::OnRosEvent(rEvent);
	sm_AchMgr.OnRosEvent(rEvent);

#if RSG_PC && RSG_ENTITLEMENT_ENABLED
	CEntitlementManager::OnRosEvent(rEvent);
#endif
}

static netStatus s_SuspendUnadvertiseStatus; 

void CLiveManager::OnServiceEvent(sysServiceEvent* evt)
{
	// early out
	if(evt == NULL)
		return;

	gnetDebug1("OnServiceEvent :: %s", evt->GetDebugName());

	// handle service events
	switch(evt->GetType())
	{

	case sysServiceEvent::SERVICE_ENTITLEMENT_UPDATED:
	case sysServiceEvent::ENTITLEMENT_UPDATED:
#if NET_ENABLE_MEMBERSHIP_FUNCTIONALITY
		rlScMembership::RequestMembershipStatus(NetworkInterface::GetLocalGamerIndex());
#endif
		break;

#if NET_ENABLE_MEMBERSHIP_FUNCTIONALITY && RSG_PROSPERO
	case sysServiceEvent::BACKGROUND_EXECUTION_EXITED:
		// check whether we should refresh membership status 
		// this is needed in cases where we purchase membership via the preview store
		if(Tunables::IsInstantiated())
		{
			// default to on, allow command line to set / unset either way
			int commandLine = 1;
			PARAM_netMembershipRefreshOnBackgroundExecutionExit.Get(commandLine);

			if(commandLine == 1)
			{
				// default to refresh in anything but prod if we have our tunables file (in prod, we don't have the preview store)
				const bool bShouldRefreshMembership_Default = (rlGetNativeEnvironment() != RL_ENV_PROD) && Tunables::GetInstance().HasCloudRequestFinished();
				bool bShouldRefreshMembership = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NET_REFRESH_MEMBERSHIP_ON_BACKGROUND_EXECUTION_EXIT", 0x5aa10006), bShouldRefreshMembership_Default);
				if(bShouldRefreshMembership)
					rlScMembership::RequestMembershipStatus(NetworkInterface::GetLocalGamerIndex());
			}
		}
		break;
#endif

	case sysServiceEvent::SUSPEND_IMMEDIATE:

		// this can happen if we launch from a suspended state - before we've initialised the network 
		// check if network session has been created 
		if(CNetwork::IsNetworkSessionValid())
			CNetwork::GetNetworkSession().OnServiceEventSuspend();
		
		if(IsOnline() && !s_SuspendUnadvertiseStatus.Pending())
		{
			if(rlScMatchManager::HasAnyMatches())
			{
				rlScMatchmaking::UnadvertiseAll(NetworkInterface::GetLocalGamerIndex(), &s_SuspendUnadvertiseStatus);
			}

			// we need to force this task through - we won't update the main thread until we resume
			// we won't succeed until we get the response, so just call this enough times to be sure
			// 5 is enough, but add some wiggle room
			// we actually cancel all tasks on resuming - including this one but it won't matter if
			// we get the http request out
			static const unsigned UNADVERTISE_UPDATE_LOOPS = 10;
			for(unsigned i = 0; i < UNADVERTISE_UPDATE_LOOPS && s_SuspendUnadvertiseStatus.Pending(); i++)
			{
				rlGetTaskManager()->UpdateTask(&s_SuspendUnadvertiseStatus);
			}
		}

		sm_bSuspendActionPending = true; 
		break;

	case sysServiceEvent::EXITING:
		// adding this here as a fallback if a suspend event isn't sent on app termination
		if(CNetwork::IsNetworkSessionValid())
			CNetwork::GetNetworkSession().OnServiceEventSuspend();
		break;

	case sysServiceEvent::RESUME_IMMEDIATE:
	case sysServiceEvent::GAME_RESUMED:
		sm_bResumeActionPending = true; 
		sm_LastResumeTimestamp = sysTimer::GetSystemMsTime();
		break;

	case sysServiceEvent::INVITATION_RECEIVED:
		break;

	case sysServiceEvent::CONSTRAINED:
		DURANGO_ONLY(CNetwork::GetVoice().OnConstrained());
		break;

	case sysServiceEvent::UNCONSTRAINED:
#if RSG_DURANGO
		if(RL_IS_VALID_LOCAL_GAMER_INDEX(NetworkInterface::GetLocalGamerIndex()))
		{
			rlFriendsManager::CheckListForChanges(NetworkInterface::GetLocalGamerIndex());
		}
		CNetwork::GetVoice().OnUnconstrained();
#endif
#if NET_ENABLE_MEMBERSHIP_FUNCTIONALITY
		// this is a back-up that we shouldn't need but added as a fail-safe
		if(Tunables::IsInstantiated() && Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NET_MEMBERSHIP_REFRESH_ON_UNCONSTRAIN", 0xe8402fae), false))
		{
			gnetDebug1("OnServiceEvent :: Unconstrain :: Requesting Membership Status");
			rlScMembership::RequestMembershipStatus(NetworkInterface::GetLocalGamerIndex());
		}
#endif
		break;

	case sysServiceEvent::APP_LAUNCHED:
		{
#if RSG_ORBIS
			unsigned nFlags = 0;
			if(g_SysService.HasParam("-StraightIntoFreemode"))
				nFlags |= LAUNCH_STRAIGHT_INTO_FREEMODE;
			if(g_SysService.HasParam("-FromLiveArea"))
				nFlags |= LAUNCH_FROM_LIVE_AREA;
			if(g_SysService.HasParam("-LiveAreaLoadContent"))
				nFlags |= LAUNCH_DIRECT_TO_CONTENT;

			gnetDebug1("OnServiceEvent :: Flags: 0x%08x, Args: %s", nFlags, g_SysService.GetArgs());

			if(!CNetwork::IsScriptReadyForEvents())
			{
				gnetDebug1("OnServiceEvent :: Script not ready. Will cache and send event when ready.");
				sm_bPendingAppLaunchScriptEvent = true;
				sm_nPendingAppLaunchFlags = nFlags;
			}
			else
			{
				// fire both events while script transition and then we can drop the app launched one
				GetEventScriptNetworkGroup()->Add(CEventNetworkSystemServiceEvent(sysServiceEvent::APP_LAUNCHED, nFlags));
				GetEventScriptNetworkGroup()->Add(CEventNetworkAppLaunched(nFlags));
			}
#endif
		}
		break;

	default:
		break;
	}
}

void CLiveManager::OnFriendEvent(const rlFriendEvent* evt)
{
	const unsigned evtId = evt->GetId();
	if (RLFRIEND_EVENT_READY == evtId)
	{
		sm_FriendsReadBuf.ClearFriendsArray();
		sm_FriendsReadBuf.m_StartIndex = 0;
		sm_bEvaluateFriendPeds			= true;	// Evaluate peds after info is done
		sm_bFriendActivatedNeedsUpdate	= true; // Get presence after info is done

		if (!sm_FriendStatus.Pending())
		{
			gnetDebug3("RLFRIEND_EVENT_READY - Reloading friends");
			int friendFlags = rlFriendsReader::FRIENDS_ALL | rlFriendsReader::FRIENDS_PRESORT_ONLINE | rlFriendsReader::FRIENDS_PRESORT_ID;
			rlFriendsManager::GetFriends(NetworkInterface::GetLocalGamerIndex(), &sm_FriendsReadBuf, friendFlags, &sm_FriendStatus);
		}

		// player card manager reset when friends manager fires a 'ready' event post-initialization
		if (SPlayerCardManager::IsInstantiated())
		{
			CPlayerCardManager& cardMgr = SPlayerCardManager::GetInstance();
			if(cardMgr.IsValid())
			{
				if( BasePlayerCardDataManager* pCardDataMgr = cardMgr.GetDataManager() )
				{
					const CPlayerCardManager::CardTypes& currType = cardMgr.GetCurrentType();
					if(currType == CPlayerCardManager::CARDTYPE_MP_FRIEND ||
						currType == CPlayerCardManager::CARDTYPE_SP_FRIEND)
					{
						pCardDataMgr->FlagForRefresh();
					}
				}
			}
		}
	}
	else if(RLFRIEND_EVENT_LIST_CHANGED == evtId)
	{
		sm_FriendsReadBuf.ClearFriendsArray();
		sm_bEvaluateFriendPeds = true;	// Evaluate peds after info is done

#if RSG_XBOX
		// Get presence after info is done - Xbox platforms only. On non-Xbox platforms,
		// presence is already included the changed event.
		sm_bFriendActivatedNeedsUpdate	= true; 
#endif

		// If we're already reading, don't bother with a new read
		if (!sm_FriendStatus.Pending())
		{
			gnetDebug3("RLFRIEND_EVENT_LIST_CHANGED - Reloading friends");
			int friendFlags = rlFriendsReader::FRIENDS_ALL | rlFriendsReader::FRIENDS_PRESORT_ONLINE | rlFriendsReader::FRIENDS_PRESORT_ID;
			rlFriendsManager::GetFriends(NetworkInterface::GetLocalGamerIndex(), &sm_FriendsReadBuf, friendFlags, &sm_FriendStatus);
		}

		// friend list updated, refresh permissions
		CNetwork::GetVoice().OnFriendListChanged(); 
	}
	else if (RLFRIEND_EVENT_NEEDS_RECHECK == evtId)
	{
		if (RL_IS_VALID_LOCAL_GAMER_INDEX(NetworkInterface::GetLocalGamerIndex()))
		{
			rlFriendsManager::CheckListForChanges(NetworkInterface::GetLocalGamerIndex());
		}
	}
	else if (RLFRIEND_EVENT_UPDATED == evtId)
	{
		rlFriendEventUpdated* e = (rlFriendEventUpdated*)evt;

#if RSG_ORBIS
		if (sm_FriendsPage.OnFriendUpdated(e->m_Friend))
		{
			sm_FriendsPage.Sort();
			if (SPlayerCardManager::IsInstantiated() && SPlayerCardManager::GetInstance().IsValid() && SPlayerCardManager::GetInstance().GetDataManager())
			{
				if(SPlayerCardManager::GetInstance().GetCurrentType() == CPlayerCardManager::CARDTYPE_MP_FRIEND ||
					SPlayerCardManager::GetInstance().GetCurrentType() == CPlayerCardManager::CARDTYPE_SP_FRIEND)
				{
					SPlayerCardManager::GetInstance().GetDataManager()->RefreshGamerData();
				}
			}
		}
#else
		sm_FriendsPage.OnFriendUpdated(e->m_Friend);
#endif
	}
	else if (RLFRIEND_EVENT_STATUS_CHANGED == evtId)
	{
		rlFriendEventStatusChanged* e = (rlFriendEventStatusChanged*)evt;
		sm_FriendsPage.OnFriendStatusChange(e->m_Friend);
	}
	else if (RLFRIEND_EVENT_TITLE_CHANGED == evtId)
	{
		rlFriendEventTitleChanged* e = (rlFriendEventTitleChanged*)evt;
		sm_FriendsPage.OnFriendTitleChange(e->m_Friend);
	}
	else if (RLFRIEND_EVENT_SESSION_CHANGED == evtId)
	{
		rlFriendEventSessionChanged* e = (rlFriendEventSessionChanged*)evt;
		sm_FriendsPage.OnFriendSessionChange(e->m_Friend);
	}
}

#if NET_ENABLE_MEMBERSHIP_FUNCTIONALITY
void CLiveManager::OnMembershipEvent(const rlScMembershipEvent* evt)
{
	switch(evt->GetId())
	{
	case RLMEMBERSHIP_EVENT_STATUS_RECEIVED:
		{
			rlScMembershipStatusReceivedEvent* e = (rlScMembershipStatusReceivedEvent*)evt;
		
			// if suppressed, cache this and send when we exit
			if(sm_SuppressMembershipForScript)
			{
				netDebug("OnMembershipEvent :: Suppressed, Caching rlScMembershipStatusReceivedEvent");
				sm_MembershipReceivedEvent = *e;
				sm_HasCachedMembershipReceivedEvent = true;
				return;
			}

			// just use an INVALID info for the previous state
			GetEventScriptNetworkGroup()->Add(CEventNetworkScMembershipStatus(
				CEventNetworkScMembershipStatus::EventType_StatusReceived,
				e->m_Info,
				rlScMembershipInfo::INVALID));
		}
		break;
	case RLMEMBERSHIP_EVENT_STATUS_CHANGED:
		{
			rlScMembershipStatusChangedEvent* e = (rlScMembershipStatusChangedEvent*)evt;

			// flag a telemetry refresh
			sm_bMembershipTelemetryPending = true;

			const bool hasChangedMembership = e->m_Info.HasMembership() != e->m_PrevInfo.HasMembership();
			const bool hasGainedMembership = e->m_Info.HasMembership() && !e->m_PrevInfo.HasMembership();

			// flag for the store to update
			if(hasGainedMembership)
				cStoreScreenMgr::OnMembershipGained();

			// if suppressed, cache this and send when we exit
			if(hasChangedMembership && sm_SuppressMembershipForScript)
			{
				netDebug("OnMembershipEvent :: Suppressed, Caching rlScMembershipStatusChangedEvent");
				sm_MembershipChangedEvent = *e;
				sm_HasCachedMembershipChangedEvent = true;
				return;
			}
			
			// if we gained membership, evaluate benefits 
			// this can be done after the suppress as we wait for script anyway
			if(!sm_NeedsToEvaluateBenefits && hasGainedMembership)
			{
				netDebug("OnMembershipEvent :: Gained Membership - Flagging EvaluateSubscriptionBenefits");
				sm_NeedsToEvaluateBenefits = true;
			}

			GetEventScriptNetworkGroup()->Add(CEventNetworkScMembershipStatus(
				CEventNetworkScMembershipStatus::EventType_StatusChanged,
				e->m_Info,
				e->m_PrevInfo));
		}
		break;
	default:
		break;
	}
}
#endif

void CLiveManager::RefreshFriendsRead()
{
	if (sm_FriendStatus.Succeeded())
	{
		sm_FriendsPage.ClearFriendsArray();
		sm_FriendsPage.m_NumFriends = sm_FriendsReadBuf.m_NumFriends;
		sm_FriendsPage.m_StartIndex = sm_FriendsReadBuf.m_StartIndex;
		for (int i = 0; i < sm_FriendsReadBuf.m_NumFriends; i++)
		{
			sm_FriendsPage.m_Friends[i] = sm_FriendsReadBuf.m_Friends[i];
		}

		sm_FriendStatus.Reset();

		// A friends read has succeeded and changes were flagged:
		// If a game is in progress, quickly iterate the peds and set their friend status
		if (sm_bEvaluateFriendPeds && NetworkInterface::IsGameInProgress())
		{
			sm_bEvaluateFriendPeds = false;

			//Iterate over the players.
			unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
			const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

			for(unsigned index = 0; index < numPhysicalPlayers; index++)
			{
				const CNetGamePlayer *pOtherPlayer = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);
				if (pOtherPlayer && pOtherPlayer->GetPlayerInfo())
				{
					bool bIsFriend = rlFriendsManager::IsFriendsWith(NetworkInterface::GetLocalGamerIndex(), pOtherPlayer->GetPlayerInfo()->m_GamerInfo.GetGamerHandle());
					pOtherPlayer->GetPlayerInfo()->m_FriendStatus = bIsFriend ? CPlayerInfo::FRIEND_ISFRIEND : CPlayerInfo::FRIEND_NOTFRIEND;
					gnetDebug3("RefreshFriendsRead :: updating player ped %s as %s", pOtherPlayer->GetPlayerInfo()->m_GamerInfo.GetName(), bIsFriend ? "FRIEND" : "NOTFRIEND");
				}
			}
		}
	}

	// After the friends manager has been initialized, we want to do a presence query to check session status
#if ALLOW_MANUAL_FRIEND_REFRESH
	if (sm_bFriendActivatedNeedsUpdate)
	{
		// Wait until we can read presence
		if (!sm_FriendStatus.Pending() && rlFriendsManager::CanQueueFriendsRefresh(NetworkInterface::GetLocalGamerIndex()))
		{
			int numToRefresh = rage::Min((int)rlPresenceQuery::MAX_SESSIONS_TO_FIND, (int)sm_FriendsPage.m_NumFriends);
			rlFriendsManager::RequestRefreshFriendsPage(NetworkInterface::GetLocalGamerIndex(), &sm_FriendsPage, sm_FriendsPage.m_StartIndex,  numToRefresh, true);

			sm_bFriendActivatedNeedsUpdate = false;
		}
	}
#endif // #if ALLOW_MANUAL_FRIEND_REFRESH
}

#if __BANK
bool CLiveManager::sm_ToggleCallback = false;
#if __WIN32PC
static char s_DownloadUrl[512] = {0};
#endif

static bkGroup* s_BankGroup = 0;
static bkGroup* s_GroupFriendsInvites = 0;

//Achievements
static const unsigned ACHIEVEMENT_LABEL = 128;
static char  s_AchLabels[MAX_ACHIEVEMENTS][ACHIEVEMENT_LABEL];
static int   s_AchIds[MAX_ACHIEVEMENTS];
static bool  s_AchIsAchieved[MAX_ACHIEVEMENTS];
static bool  s_AwardAch[MAX_ACHIEVEMENTS];

#if __STEAM_BUILD
static int s_SteamAchProgress = 0;
static int s_SteamAchId;
#endif

static unsigned s_PresenceId;
#if RSG_NP
static char s_PresenceStatus[RL_PRESENCE_MAX_BUF_SIZE];
#endif

//Privileges
static const unsigned	MAX_PRIVILEGE_DISPLAY_STRING_LENGTH = 64;
static char				s_PrivilegesDisplayString[MAX_PRIVILEGE_DISPLAY_STRING_LENGTH] = "";
static int				s_PrivilegeCheckLocalGamerIndex = 0;
static bool				s_PrivilegeCheckAttemptResolution = false;
static bool				s_PrivilegesToCheck[rlPrivileges::PRIVILEGE_MAX];

//System UI
static const unsigned	MAX_SYSTEMUI_DISPLAY_STRING_LENGTH = 64;
static char				s_SystemUiDisplayString[MAX_SYSTEMUI_DISPLAY_STRING_LENGTH] = "";

//Friends/Invites
static const char*		s_Friends[rlFriendsPage::MAX_FRIEND_PAGE_SIZE];
static unsigned int		s_NumFriends = 0;
static const char*		s_Invites[10];
static unsigned int		s_NumInvites = 0;
//invites
static bkCombo*     s_InviteCombo = 0;
static int          s_InviteComboIndex = 0;
//friends
static bkCombo*     s_FriendCombo = 0;
static int          s_FriendComboIndex = 0;
static bkButton*	s_InviteButton = 0;
// friend pages
#if RSG_ORBIS || RSG_DURANGO
static int			s_FriendPageIndex = 0;
static int			s_FriendPageSize = 1;
#endif
#if RSG_XBOX
static int s_CharacterIndex = 0;
static int s_StatInt = 0;
static float s_StatFloat = 0.0f;
#endif

static char s_ProfanityString[netProfanityFilter::MAX_STRING_SIZE];
netProfanityFilter::CheckTextToken s_profanityToken;
void Bank_DoProfanifyCheck()
{
	netProfanityFilter::VerifyStringForProfanity(s_ProfanityString, CText::GetScLanguageFromCurrentLanguageSetting(), false, s_profanityToken);
}
void Bank_CheckProfanityToken()
{
	netProfanityFilter::ReturnCode retCode = netProfanityFilter::GetStatusForRequest(s_profanityToken);
	static const char* returnCodeStrings[] = {"OK", "FAILED", "PENDING", "TOKEN_INVALID"};
	gnetDebug1("ProfanityStatus %s", retCode >= 0 && (int)retCode < NELEM(returnCodeStrings) ? returnCodeStrings[retCode] : "ERROR" );
}

static
void NetworkBank_FakeSuspend()
{
	gnetDebug1("Triggering fake suspend event");

#if RSG_ORBIS
	g_SysService.FakeEvent(sysServiceEvent::SUSPENDED);
#else
	sysServiceEvent e(sysServiceEvent::SUSPENDED);
	g_SysService.TriggerEvent(&e);
#endif
}

static
void NetworkBank_FakeSuspendImmediate()
{
	gnetDebug1("Triggering fake suspend immediate event");

#if RSG_ORBIS
	g_SysService.FakeEvent(sysServiceEvent::SUSPEND_IMMEDIATE);
#else
	sysServiceEvent e(sysServiceEvent::SUSPEND_IMMEDIATE);
	g_SysService.TriggerEvent(&e);
#endif
}

static
void NetworkBank_FakeResume()
{
	gnetDebug1("Triggering fake resume event");

#if RSG_ORBIS
	g_SysService.FakeEvent(sysServiceEvent::GAME_RESUMED);
#else
	sysServiceEvent e(sysServiceEvent::RESUME_IMMEDIATE);
	g_SysService.TriggerEvent(&e);
#endif
}

static char g_szFakeAppLaunchFlags[sysService::MAX_ARG_LENGTH];
static char g_szBase64Text[sysService::MAX_ARG_LENGTH];
static char g_szBase64CovertedText[sysService::MAX_ARG_LENGTH];

static
void NetworkBank_FakeAppLaunch()
{
	gnetDebug1("NetworkBank_FakeAppLaunch :: Faking APP_LAUNCHED - Args: %s", g_szFakeAppLaunchFlags);

#if RSG_ORBIS
	if(datBase64::IsValidBase64String(g_szFakeAppLaunchFlags))
	{
		// we need to decode this to get the real args
		unsigned char decodedArgs[sysService::MAX_ARG_LENGTH];
		unsigned decodedLen = 0;
		datBase64::Decode(g_szFakeAppLaunchFlags, sysService::MAX_ARG_LENGTH, decodedArgs, &decodedLen);
		
		gnetDebug1("NetworkBank_FakeAppLaunch :: Detected Base64 - Decoded Args: %s", decodedArgs);
		g_SysService.SetArgs((char*)decodedArgs);
	}
	else
	{
		g_SysService.SetArgs(g_szFakeAppLaunchFlags);
	}

	g_SysService.FakeEvent(sysServiceEvent::APP_LAUNCHED);
#else
	sysServiceEvent e(sysServiceEvent::APP_LAUNCHED);
	g_SysService.TriggerEvent(&e);
#endif
}

#if RL_NP_SUPPORT_PLAY_TOGETHER
static void Network_BankFakePlayTogether()
{
	gnetDebug1("Network_BankFakePlayTogether");

	// invitee group
	rlSceNpOnlineId aInvitees[RL_MAX_PLAY_TOGETHER_GROUP];
	unsigned nInvitees = 0; 

	// update the friend list
	unsigned nFriends = CLiveManager::GetFriendsPage()->m_NumFriends;
	for(u32 i = 0; (i < nFriends) && (nInvitees < RL_MAX_PLAY_TOGETHER_GROUP); i++)
	{
		rlGamerHandle hGamer;
		CLiveManager::GetFriendsPage()->m_Friends[i].GetGamerHandle(&hGamer);
		if(hGamer.IsValid())
			aInvitees[nInvitees++] = hGamer.GetNpOnlineId();
	}

	if(nInvitees > 0)
	{
		// dispatch an event via NP
		rlNpEventPlayTogetherHost e(NetworkInterface::GetLocalGamerIndex(), nInvitees, aInvitees);
		g_rlNp.DispatchEvent(&e);
	}
}
#endif

static
void NetworkBank_EncodeBase64()
{
	unsigned encodedLength = 0;
	datBase64::Encode(reinterpret_cast<u8*>(g_szBase64Text), static_cast<unsigned>(StringLength(g_szBase64Text)), g_szBase64CovertedText, sysService::MAX_ARG_LENGTH, &encodedLength);
}

static
void NetworkBank_DecodeBase64()
{
	unsigned decodedLength = 0;
	datBase64::Decode(g_szBase64Text, sysService::MAX_ARG_LENGTH, reinterpret_cast<u8*>(g_szBase64CovertedText), &decodedLength);
}

static
void NetworkBank_AcceptInvite()
{
    CLiveManager::GetInviteMgr().AcceptInvite(s_InviteComboIndex);
}

static
void NetworkBank_InviteFriend()
{
	const int index = CLiveManager::GetFriendsPage()->GetFriendIndex(&s_Friends[s_FriendComboIndex][0]);
	if (index < CLiveManager::GetFriendsPage()->m_NumFriends)
	{
		rlFriend& f = CLiveManager::GetFriendsPage()->m_Friends[index];

		rlGamerHandle hGamer;
		f.GetGamerHandle(&hGamer);

		if (gnetVerify(NetworkInterface::IsGameInProgress()))
		{
			char salutation[100];

			const NetworkGameConfig& matchConfig = NetworkInterface::GetMatchConfig();

			sprintf(salutation, "%d %d", matchConfig.GetGameMode(), CGameLogic::GetCurrentLevelIndex());

			if (!CNetwork::GetNetworkSession().SendInvites(&hGamer, 1))
			{
				gnetAssertf(0, "Invite Failed");
			}
		}
	}
}

static
void NetworkBank_ShowFriendUI()
{
	const int index = CLiveManager::GetFriendsPage()->GetFriendIndex(&s_Friends[s_FriendComboIndex][0]);
	if (index < CLiveManager::GetFriendsPage()->m_NumFriends)
	{
		rlFriend& f = CLiveManager::GetFriendsPage()->m_Friends[index];
		rlGamerHandle hGamer;
		f.GetGamerHandle(&hGamer);
		CLiveManager::ShowGamerProfileUi(hGamer);
	}
}

#if RSG_XBOX || RSG_PC
static
void NetworkBank_FindFriendGamerTag()
{
	const int index = CLiveManager::GetFriendsPage()->GetFriendIndex(s_Friends[s_FriendComboIndex]);
	if (index < CLiveManager::GetFriendsPage()->m_NumFriends)
	{
		rlFriend& f = CLiveManager::GetFriendsPage()->m_Friends[index];
		rlGamerHandle hGamer;
		f.GetGamerHandle(&hGamer);
		s_WidgetFindGamerTag = CLiveManager::GetFindGamerTag().Start(hGamer);
	}
}
#endif

#if RSG_DURANGO || RSG_ORBIS
static
void NetworkBank_FindFriendsDisplayNames()
{
	unsigned numFriends = CLiveManager::GetFriendsPage()->m_NumFriends;
	if(numFriends > CDisplayNamesFromHandles::MAX_DISPLAY_NAMES_PER_REQUEST)
	{
		numFriends = CDisplayNamesFromHandles::MAX_DISPLAY_NAMES_PER_REQUEST;
	}

	rlGamerHandle hGamers[CDisplayNamesFromHandles::MAX_DISPLAY_NAMES_PER_REQUEST];
	for(unsigned i = 0; i < numFriends; ++i)
	{
		rlFriend& f = CLiveManager::GetFriendsPage()->m_Friends[i];
		f.GetGamerHandle(&hGamers[i]);
	}

	if(numFriends > 0)
	{
		for(unsigned i = 0; i < COUNTOF(s_WidgetDisplayNameRequestId); ++i)
		{
			if(s_WidgetDisplayNameRequestId[i] < 0)
			{
				s_WidgetDisplayNameRequestId[i] = CLiveManager::GetFindDisplayName().RequestDisplayNames(NetworkInterface::GetLocalGamerIndex(), hGamers, numFriends);
				if(i == 7)
				{
					// simulate someone abandoning a request
					s_WidgetDisplayNameRequestId[i] = CDisplayNamesFromHandles::INVALID_REQUEST_ID;
				}
			}
		}
	}
}
#endif

static
void NetworkBank_DownloadFriendAttributes()
{
	const int index = CLiveManager::GetFriendsPage()->GetFriendIndex(&s_Friends[s_FriendComboIndex][0]);
	if (index < CLiveManager::GetFriendsPage()->m_NumFriends)
	{
		rlFriend& f = CLiveManager::GetFriendsPage()->m_Friends[index];
		rlGamerHandle hGamer;
		f.GetGamerHandle(&hGamer);
		NETWORK_GAMERS_ATTRS.Download(hGamer, CNetworkGamerAttributes::Attr_GamerTag|CNetworkGamerAttributes::Attr_ScNickname|CNetworkGamerAttributes::Attr_CrewId);
	}
}

static void NetworkBank_FakePlatformInvite()
{
	rlGamerHandle inviter;
	inviter.FromUserId("123456778");

	rlPresenceEventInviteAccepted event(
		rlPresence::GetActingGamerInfo(),
		inviter,
		rlSessionInfo(),
		0);

	CLiveManager::GetInviteMgr().HandleEventPlatformInviteAccepted(&event);
}

#if RSG_XBOX || RSG_SCE
static void NetworkBank_GetFriendPage()
{
	rlFriendsManager::RequestRefreshFriendsPage(NetworkInterface::GetLocalGamerIndex(),CLiveManager::GetFriendsPage(),0,CLiveManager::GetFriendsPage()->m_MaxFriends, false);
}
#endif

#if __WIN32PC
static void NetworkBank_PipeDownloadMessage()
{
	g_rlPc.GetDownloaderPipe()->StartDownloadWithToken("TestPlatform", "TestMessage");
}
static void NetworkBank_PipeDownloadURL()
{
	g_rlPc.GetDownloaderPipe()->StartDownloadWithUrl("TestPlatform", s_DownloadUrl);
}
static void NetworkBank_ReloadOffline()
{
	if (g_rlPc.GetUiInterface())
	{
		g_rlPc.GetUiInterface()->ReloadUi(true);
	}
}
static void NetworkBank_ReloadOnline()
{
	if (g_rlPc.GetUiInterface())
	{
		g_rlPc.GetUiInterface()->ReloadUi(false);
	}
}
#endif

static void NetworkBank_CheckPrivileges()
{
	int privilegeTypeBitfield = 0;

	for (int i = 0; i < rlPrivileges::PRIVILEGE_MAX; ++i)
	{
		if (s_PrivilegesToCheck[i])
		{
			privilegeTypeBitfield |= rlPrivileges::GetPrivilegeBit(static_cast<rlPrivileges::PrivilegeTypes>(i));
		}
	}

	if (rlPrivileges::CheckPrivileges(s_PrivilegeCheckLocalGamerIndex, privilegeTypeBitfield, s_PrivilegeCheckAttemptResolution))
	{
		strncpy(s_PrivilegesDisplayString, "Privilege Check start succeeded.", MAX_PRIVILEGE_DISPLAY_STRING_LENGTH-1);
	}
	else
	{
		strncpy(s_PrivilegesDisplayString, "Privilege Check start failed.", MAX_PRIVILEGE_DISPLAY_STRING_LENGTH-1);
	}
}

static void NetworkBank_IsPrivilegeCheckResultReady()
{
	if (rlPrivileges::IsPrivilegeCheckResultReady())
	{
		strncpy(s_PrivilegesDisplayString, "Privilege Check Result is ready.", MAX_PRIVILEGE_DISPLAY_STRING_LENGTH-1);
	}
	else
	{
		strncpy(s_PrivilegesDisplayString, "Privilege Check Result is not ready.", MAX_PRIVILEGE_DISPLAY_STRING_LENGTH-1);
	}
}

static void NetworkBank_IsPrivilegeCheckInProgress()
{
	if (rlPrivileges::IsPrivilegeCheckInProgress())
	{
		strncpy(s_PrivilegesDisplayString, "Privilege Check is in progress.", MAX_PRIVILEGE_DISPLAY_STRING_LENGTH-1);
	}
	else
	{
		strncpy(s_PrivilegesDisplayString, "Privilege Check Result is not in progress.", MAX_PRIVILEGE_DISPLAY_STRING_LENGTH-1);
	}
}

static void NetworkBank_IsPrivilegeCheckSuccessful()
{
	if (rlPrivileges::IsPrivilegeCheckSuccessful())
	{
		strncpy(s_PrivilegesDisplayString, "Privilege Check is successful.", MAX_PRIVILEGE_DISPLAY_STRING_LENGTH-1);
	}
	else
	{
		strncpy(s_PrivilegesDisplayString, "Privilege Check Result is not successful.", MAX_PRIVILEGE_DISPLAY_STRING_LENGTH-1);
	}
}

static void NetworkBank_SetPrivilegeCheckResultNotNeeded()
{
	rlPrivileges::SetPrivilegeCheckResultNotNeeded();
	strncpy(s_PrivilegesDisplayString, "Privilege Check Result has been set to not needed.", MAX_PRIVILEGE_DISPLAY_STRING_LENGTH-1);
}

static void NetworkBank_IsSystemUiShowing()
{
	if(CLiveManager::IsSystemUiShowing())
	{
		formatf(s_SystemUiDisplayString, "System UI is showing");
	}
	else
	{
		formatf(s_SystemUiDisplayString, "System UI is not showing");
	}
}

static void NetworkBank_ShowSignInUi()
{
	if(CLiveManager::ShowSigninUi(rlSystemUi::SIGNIN_NONE))
	{
		formatf(s_SystemUiDisplayString, "Sign In UI Displayed Successfully");
	}
	else
	{
		formatf(s_SystemUiDisplayString, "Sign In UI Displayed Unsuccessfully");
	}
}

static void NetworkBank_ShowPartyUi()
{
	if(CLiveManager::ShowPlatformPartyUi())
	{
		formatf(s_SystemUiDisplayString, "Party UI Displayed Successfully");
	}
	else
	{
		formatf(s_SystemUiDisplayString, "Party UI Displayed Unsuccessfully");
	}
}

static void NetworkBank_ShowSendInviteUi()
{
	if(CLiveManager::SendPlatformPartyInvites())
	{
		formatf(s_SystemUiDisplayString, "Send Invite UI Displayed Successfully");
	}
	else
	{
		formatf(s_SystemUiDisplayString, "Send Invite UI Displayed Unsuccessfully");
	}
}

void NetworkBank_ShowAccountUpgradeUi()
{
	if (CLiveManager::ShowAccountUpgradeUI())
	{
		formatf(s_SystemUiDisplayString, "Account Upgrade UI Displayed Successfully");
	}
	else
	{
		formatf(s_SystemUiDisplayString, "Account Upgrade UI Displayed Unsuccessfully");
	}
}

void NetworkBank_YoutubeOauth()
{
	static rlYoutubeUpload* upload = nullptr;
	static netStatus status;

	if (upload == nullptr)
	{
		upload = rage_new rlYoutubeUpload();
	}

	if (!status.Pending())
		rlYoutube::GetOAuthToken(NetworkInterface::GetLocalGamerIndex(), upload, &status);
}

void NetworkBank_ShowYoutubeUi()
{
	static netStatus status;
	if (!status.Pending())
		rlYoutube::ShowAccountLinkUi(NetworkInterface::GetLocalGamerIndex(), "gtav", "replay", &status);
}

#if RSG_ORBIS
void PS4Bank_CheckPresenceConnection()
{
	bool bConnected = g_rlNp.IsConnectedToPresence(NetworkInterface::GetLocalGamerIndex());
	Displayf("NP Presence Connection: %s", bConnected ? "connected" : "not connected" );
}

void PS4Bank_ShowPS4InviteSend()
{
	if (CNetwork::IsNetworkSessionValid())
	{
		int userId = g_rlNp.GetUserServiceId(NetworkInterface::GetLocalGamerIndex());
		g_rlNp.GetCommonDialog().SendInvitation(userId, CNetwork::GetNetworkSession().GetSnSession().GetWebApiSessionId(), "Play with me!");
	}
}

void PS4Bank_ShowPS4InviteReceive()
{
	int userId = g_rlNp.GetUserServiceId(NetworkInterface::GetLocalGamerIndex());
	g_rlNp.GetCommonDialog().ReceiveInvitation(userId);
}

void PS4Bank_PSPlusUpsell()
{
	int userId = g_rlNp.GetUserServiceId(NetworkInterface::GetLocalGamerIndex());
	g_rlNp.GetCommonDialog().ShowCommercePSPlusDialog(userId);
}

void PS4Bank_RefreshPermissions()
{
	if(!gnetVerifyf(RL_IS_VALID_LOCAL_GAMER_INDEX(NetworkInterface::GetLocalGamerIndex()), "Need to be signed in to use this widget!"))
		return;

	if(g_rlNp.CanRequestPermissions(NetworkInterface::GetLocalGamerIndex()))
		g_rlNp.GetNpAuth().RequestPermissions(NetworkInterface::GetLocalGamerIndex());
}

void PS4Bank_AccountIdToOnlineId()
{
    static rlSceNpOnlineId s_OnlineId;
    static netStatus s_OnlineIdStatus;
    g_rlNp.GetWebAPI().GetOnlineId(NetworkInterface::GetLocalGamerIndex(), g_rlNp.GetNpAccountId(NetworkInterface::GetLocalGamerIndex()), &s_OnlineId, &s_OnlineIdStatus);
}

void PS4Bank_OnlineIdToAccountId()
{
    static rlSceNpAccountId s_AccountId;
    static netStatus s_AccountIdStatus;
    g_rlNp.GetWebAPI().GetAccountId(NetworkInterface::GetLocalGamerIndex(), g_rlNp.GetNpOnlineId(NetworkInterface::GetLocalGamerIndex()), &s_AccountId, &s_AccountIdStatus);
}

void PS4Bank_ShowAccountIdGamerProfile()
{
    rlGamerHandle gamer;
    gamer.ResetNp(g_rlNp.GetEnvironment(), g_rlNp.GetNpAccountId(NetworkInterface::GetLocalGamerIndex()), nullptr);
    CLiveManager::ShowGamerProfileUi(gamer);
}

#elif RSG_DURANGO

void XB1Bank_WriteCashEarnedMP()
{
	events_durango::WriteEvent_CashEarnedMP(s_StatInt);
}

void XB1Bank_EnemyDefeatedSP()
{
	events_durango::WriteEvent_EnemyDefeatedSP(1);
}

void XB1Bank_HeldUpShops()
{
	events_durango::WriteEvent_HeldUpShops(s_StatInt);
}

void XB1Bank_HordeWavesSurvived()
{
	events_durango::WriteEvent_HordeWavesSurvived(s_StatInt);
}

void XB1Bank_MoneyTotalSpent()
{
	events_durango::WriteEvent_MoneyTotalSpent(s_CharacterIndex, s_StatInt);
}

void XB1Bank_MultiplayerRoundEnd()
{
	//events_durango::WriteEvent_MultiplayerRoundEnd();
}

void XB1Bank_MultiplayerRoundStart()
{
	//events_durango::WriteEvent_MultiplayerRoundStart();
}

void XB1Bank_PlatinumAwards()
{
	events_durango::WriteEvent_PlatinumAwards(s_StatInt);
}

void XB1Bank_PlayerDefeatedSP()
{
	events_durango::WriteEvent_PlayerDefeatedSP(1);
}

void XB1Bank_PlayerKillsMP()
{
	events_durango::WriteEvent_PlayerKillsMP(1);
}

void XB1Bank_PlayerSessionEnd()
{
	events_durango::WriteEvent_PlayerSessionEnd();
}

void XB1Bank_PlayerSessionPause()
{
	events_durango::WriteEvent_PlayerSessionPause();
}

void XB1Bank_PlayerSessionResume()
{
	events_durango::WriteEvent_PlayerSessionResume();
}

void XB1Bank_PlayerSessionStart()
{
	events_durango::WriteEvent_PlayerSessionStart();
}

void XB1Bank_PlayerRespawnedSP()
{
	events_durango::WriteEvent_PlayerRespawnedSP();
}

void XB1Bank_TotalCustomRacesWon()
{
	events_durango::WriteEvent_TotalCustomRacesWon(s_StatInt);
}

//////////////////////////////////////////////////////////////
// Hero Bank Events
//////////////////////////////////////////////////////////////
void XB1Bank_GameProgressSP()
{
	events_durango::WriteEvent_GameProgressSP(s_StatFloat);
}

void XB1Bank_PlayingTime()
{
	events_durango::WriteEvent_PlayingTime(s_StatInt);
}

void XB1Bank_CashEarnedSP()
{
	events_durango::WriteEvent_CashEarnedSP(s_StatInt);
}

void XB1Bank_StolenCars()
{
	events_durango::WriteEvent_StolenCars(s_StatInt);
}

void XB1Bank_StarsAttained()
{
	events_durango::WriteEvent_StarsAttained(s_StatInt);
}

void XB1Bank_MultiplayerPlaytime()
{
	events_durango::WriteEvent_MultiplayerPlaytime(s_StatInt);
}

void XB1Bank_CashEarnedMP()
{
	events_durango::WriteEvent_CashEarnedMP(s_StatInt);
}

void XB1Bank_KillDeathRatio()
{
	events_durango::WriteEvent_KillDeathRatio(s_StatFloat);
}

void XB1Bank_ArchEnemy()
{
	// 2533274993784006 - Test User: RKTREOOTANGEDMX
	events_durango::WriteEvent_ArchEnemy(2533274993784006);
}

void XB1Bank_HighestRankMP()
{
	events_durango::WriteEvent_HighestRankMP(s_StatInt);
}

void XB1Bank_GetActiveContext()
{
	static rlXblUserStatiticsInfo sUserStatInfo[10];
	static netStatus sUserStatStatus;
	sUserStatInfo[0].SetName("ASCTDEATHMLOC");
	sUserStatInfo[1].SetName("ASCTMPMISSIONS");
	sUserStatInfo[2].SetName("ASCTSPMISSIONS");
	sUserStatInfo[3].SetName("ASCTSPMG");
	sUserStatInfo[4].SetName("ASCTSPRC");
	sUserStatInfo[5].SetName("CTDEATHMLOC");
	sUserStatInfo[6].SetName("CTMPMISSIONS");
	sUserStatInfo[7].SetName("CTSPMISSIONS");
	sUserStatInfo[8].SetName("CTSPMG");
	sUserStatInfo[9].SetName("CTSPRC");

	if (!sUserStatStatus.Pending())
	{
		IUserStatistics::GetUserStatistics(NetworkInterface::GetLocalGamerIndex(), &(sUserStatInfo[0]), 10, SERVICE_ID_TITLE, &sUserStatStatus);
	}
}

void WriteActiveContext(int presenceId, const char* fieldStat)
{
	richPresenceFieldStat rpFieldStat;
	rpFieldStat.m_PresenceId = presenceId;
	safecpy(rpFieldStat.m_FieldStat, fieldStat);

	rlGamerInfo gamerInfo;
	rlPresence::GetGamerInfo(NetworkInterface::GetLocalGamerIndex(), &gamerInfo);
	events_durango::WriteActiveContext(gamerInfo, &rpFieldStat, 1);
}

void XB1Bank_ActiveContext1()
{
	WriteActiveContext(1, "SPM_0");
}

void XB1Bank_ActiveContext5()
{
	WriteActiveContext(5, "DM_LOC_0");
}

void XB1Bank_ActiveContext7()
{
	WriteActiveContext(7, "MPM_12");
}

void XB1Bank_ActiveContext8()
{
	WriteActiveContext(8, "SPMG_0");
}

void XB1Bank_ActiveContext9()
{
	WriteActiveContext(9, "SPRC_0");
}

void XB1Bank_RefreshParty()
{
	gnetDebug1("Refreshing Party via XB1Bank_RefreshParty");
	g_rlXbl.GetPartyManager()->FlagRefreshParty();
}

#elif RSG_SCARLETT

void ScarlettBank_WriteCashEarnedMP()
{
	events_scarlett::WriteEvent_CashEarnedMP(s_StatInt);
}

void ScarlettBank_EnemyDefeatedSP()
{
	events_scarlett::WriteEvent_EnemyDefeatedSP(1);
}

void ScarlettBank_HeldUpShops()
{
	events_scarlett::WriteEvent_HeldUpShops(s_StatInt);
}

void ScarlettBank_HordeWavesSurvived()
{
	events_scarlett::WriteEvent_HordeWavesSurvived(s_StatInt);
}

void ScarlettBank_MoneyTotalSpent()
{
	events_scarlett::WriteEvent_MoneyTotalSpent(s_CharacterIndex, s_StatInt);
}

void ScarlettBank_MultiplayerRoundEnd()
{
	//events_scarlett::WriteEvent_MultiplayerRoundEnd();
}

void ScarlettBank_MultiplayerRoundStart()
{
	//events_scarlett::WriteEvent_MultiplayerRoundStart();
}

void ScarlettBank_PlatinumAwards()
{
	events_scarlett::WriteEvent_PlatinumAwards(s_StatInt);
}

void ScarlettBank_PlayerDefeatedSP()
{
	events_scarlett::WriteEvent_PlayerDefeatedSP(1);
}

void ScarlettBank_PlayerKillsMP()
{
	events_scarlett::WriteEvent_PlayerKillsMP(1);
}

void ScarlettBank_PlayerSessionEnd()
{
	events_scarlett::WriteEvent_PlayerSessionEnd();
}

void ScarlettBank_PlayerSessionPause()
{
	events_scarlett::WriteEvent_PlayerSessionPause();
}

void ScarlettBank_PlayerSessionResume()
{
	events_scarlett::WriteEvent_PlayerSessionResume();
}

void ScarlettBank_PlayerSessionStart()
{
	events_scarlett::WriteEvent_PlayerSessionStart();
}

void ScarlettBank_PlayerRespawnedSP()
{
	events_scarlett::WriteEvent_PlayerRespawnedSP();
}

void ScarlettBank_TotalCustomRacesWon()
{
	events_scarlett::WriteEvent_TotalCustomRacesWon(s_StatInt);
}

//////////////////////////////////////////////////////////////
// Hero Bank Events
//////////////////////////////////////////////////////////////
void ScarlettBank_GameProgressSP()
{
	events_scarlett::WriteEvent_GameProgressSP(s_StatFloat);
}

void ScarlettBank_PlayingTime()
{
	events_scarlett::WriteEvent_PlayingTime(s_StatInt);
}

void ScarlettBank_CashEarnedSP()
{
	events_scarlett::WriteEvent_CashEarnedSP(s_StatInt);
}

void ScarlettBank_StolenCars()
{
	events_scarlett::WriteEvent_StolenCars(s_StatInt);
}

void ScarlettBank_StarsAttained()
{
	events_scarlett::WriteEvent_StarsAttained(s_StatInt);
}

void ScarlettBank_MultiplayerPlaytime()
{
	events_scarlett::WriteEvent_MultiplayerPlaytime(s_StatInt);
}

void ScarlettBank_CashEarnedMP()
{
	events_scarlett::WriteEvent_CashEarnedMP(s_StatInt);
}

void ScarlettBank_KillDeathRatio()
{
	events_scarlett::WriteEvent_KillDeathRatio(s_StatFloat);
}

void ScarlettBank_ArchEnemy()
{
	// 2533274993784006 - Test User: RKTREOOTANGEDMX
	events_scarlett::WriteEvent_ArchEnemy(2533274993784006);
}

void ScarlettBank_HighestRankMP()
{
	events_scarlett::WriteEvent_HighestRankMP(s_StatInt);
}

void ScarlettBank_GetActiveContext()
{
	static rlXblUserStatiticsInfo sUserStatInfo[10];
	static netStatus sUserStatStatus;
	sUserStatInfo[0].SetName("ASCTDEATHMLOC");
	sUserStatInfo[1].SetName("ASCTMPMISSIONS");
	sUserStatInfo[2].SetName("ASCTSPMISSIONS");
	sUserStatInfo[3].SetName("ASCTSPMG");
	sUserStatInfo[4].SetName("ASCTSPRC");
	sUserStatInfo[5].SetName("CTDEATHMLOC");
	sUserStatInfo[6].SetName("CTMPMISSIONS");
	sUserStatInfo[7].SetName("CTSPMISSIONS");
	sUserStatInfo[8].SetName("CTSPMG");
	sUserStatInfo[9].SetName("CTSPRC");

	if (!sUserStatStatus.Pending())
	{
		IUserStatistics::GetUserStatistics(NetworkInterface::GetLocalGamerIndex(), &(sUserStatInfo[0]), 10, SERVICE_ID_TITLE, &sUserStatStatus);
	}
}

void WriteActiveContext(int presenceId, const char* fieldStat)
{
	richPresenceFieldStat rpFieldStat;
	rpFieldStat.m_PresenceId = presenceId;
	safecpy(rpFieldStat.m_FieldStat, fieldStat);

	rlGamerInfo gamerInfo;
	rlPresence::GetGamerInfo(NetworkInterface::GetLocalGamerIndex(), &gamerInfo);
	events_scarlett::WriteActiveContext(gamerInfo, &rpFieldStat, 1);
}

void ScarlettBank_ActiveContext1()
{
	WriteActiveContext(1, "SPM_0");
}

void ScarlettBank_ActiveContext5()
{
	WriteActiveContext(5, "DM_LOC_0");
}

void ScarlettBank_ActiveContext7()
{
	WriteActiveContext(7, "MPM_12");
}

void ScarlettBank_ActiveContext8()
{
	WriteActiveContext(8, "SPMG_0");
}

void ScarlettBank_ActiveContext9()
{
	WriteActiveContext(9, "SPRC_0");
}

void ScarlettBank_RefreshParty()
{
	gnetDebug1("Refreshing Party via ScarlettBank_RefreshParty");
	g_rlXbl.GetPartyManager()->FlagRefreshParty();
}

#endif

static bool s_BankInSpectatorMode = false;

void
CLiveManager::InitWidgets()
{
	bkBank* pBank = BANKMGR.FindBank("Network");

	if (gnetVerify(pBank) && !s_BankGroup)
	{
		pBank->PushGroup("Cloud", false);
			CloudManager::GetInstance().InitWidgets(pBank);
			UserContentManager::GetInstance().InitWidgets(pBank);
			sm_scGamerDataMgr.CreateDebugWidgets(pBank);
		pBank->PopGroup();

		sysMemSet(s_AchIds, 0, MAX_ACHIEVEMENTS);
		sysMemSet(s_AchIsAchieved, 0, MAX_ACHIEVEMENTS);
		for (unsigned i=0; i<MAX_ACHIEVEMENTS; i++)
		{
			sysMemSet(s_AchLabels[i], 0, ACHIEVEMENT_LABEL);
		}

#if __WIN32PC
		pBank->PushGroup("PC Pipe");
		pBank->AddButton("DOWN: Message", datCallback(NetworkBank_PipeDownloadMessage));
		pBank->AddText("Download URL", s_DownloadUrl, 511, false);
		pBank->AddButton("DOWNURL Message", datCallback(NetworkBank_PipeDownloadURL));
		pBank->AddButton("Go Offline", datCallback(NetworkBank_ReloadOffline));
		pBank->AddButton("Go Online", datCallback(NetworkBank_ReloadOnline));
		pBank->PopGroup();
#endif

		bkGroup *debugGroup = static_cast<bkGroup*>(BANKMGR.FindWidget("Network/Debug/Debug Live"));

        if(gnetVerify(debugGroup))
        {
            bool resetCurrentGroupAtEnd = false;

            if(debugGroup != pBank->GetCurrentGroup())
            {
                pBank->SetCurrentGroup(*debugGroup);

                resetCurrentGroupAtEnd = true;
            }

			s_BankGroup = pBank->PushGroup("Debug Live Manager", false);
		    {
				s_GroupFriendsInvites = pBank->PushGroup("Friends and Invites", false);
	                s_InviteCombo = pBank->AddCombo ("Invites", &s_InviteComboIndex, s_NumInvites, (const char **)s_Invites);
	                pBank->AddButton("Accept Invite", datCallback(NetworkBank_AcceptInvite));
					s_FriendCombo = pBank->AddCombo ("Friend", &s_FriendComboIndex, s_NumFriends, (const char **)s_Friends);
					pBank->AddButton("Show Friend UI", datCallback(NetworkBank_ShowFriendUI));
					pBank->AddButton("Download Friend Attributes", datCallback(NetworkBank_DownloadFriendAttributes));
					pBank->AddButton("Fake Platform Invite", datCallback(NetworkBank_FakePlatformInvite));

#if RSG_XBOX || RSG_PC
					pBank->AddButton("Find Friend GamerTag", datCallback(NetworkBank_FindFriendGamerTag));
#endif

#if RSG_DURANGO || RSG_ORBIS
					pBank->AddButton("Find Friends Display Names", datCallback(NetworkBank_FindFriendsDisplayNames));
#endif

#if RSG_DURANGO || RSG_ORBIS
					pBank->AddSlider("Friends Page", &s_FriendPageIndex, 0, 10, 1);
					pBank->AddSlider("Page Size", &s_FriendPageSize, 1, 50, 1);
					pBank->AddButton("Get Friends Page", datCallback(NetworkBank_GetFriendPage));
#endif
				pBank->PopGroup();   //Session

			    pBank->PushGroup("Presence", false);
			    {
					pBank->AddSlider("Presence Id", &s_PresenceId, 0, player_schema::PRESENCE_COUNT, 1);
#if RSG_NP
					pBank->AddText("Presence String", s_PresenceStatus, RL_PRESENCE_MAX_BUF_SIZE);
#endif
				    pBank->AddToggle("Set presence status", &sm_ToggleCallback, LiveBank_TestPresence);
				    pBank->AddToggle("Test Exhaustion", &sm_ToggleCallback, LiveBank_TestPresenceExhaustion);
			    }
			    pBank->PopGroup();

				pBank->PushGroup("Privileges", false);
				{
					pBank->AddText("Test Result:", &s_PrivilegesDisplayString[0], MAX_PRIVILEGE_DISPLAY_STRING_LENGTH, true);
					
					pBank->AddSeparator("");
					
					pBank->AddButton("Check Privileges", datCallback(NetworkBank_CheckPrivileges));
					pBank->AddSlider("Local Gamer Index", &s_PrivilegeCheckLocalGamerIndex, -3, rage::RL_MAX_LOCAL_GAMERS, 1);
					pBank->AddToggle("Attempt Resolution", &s_PrivilegeCheckAttemptResolution);
					pBank->PushGroup("Privileges to Check", false);
					{
#if RSG_DURANGO
						pBank->AddToggle("ADD_FRIEND", &s_PrivilegesToCheck[rlPrivileges::PRIVILEGE_ADD_FRIEND]);
						pBank->AddToggle("CLOUD_GAMING_JOIN_SESSION", &s_PrivilegesToCheck[rlPrivileges::PRIVILEGE_CLOUD_GAMING_JOIN_SESSION]);
						pBank->AddToggle("CLOUD_GAMING_MANAGE_SESSION", &s_PrivilegesToCheck[rlPrivileges::PRIVILEGE_CLOUD_GAMING_MANAGE_SESSION]);
						pBank->AddToggle("CLOUD_SAVED_GAMES", &s_PrivilegesToCheck[rlPrivileges::PRIVILEGE_CLOUD_SAVED_GAMES]);
						pBank->AddToggle("COMMUNICATIONS", &s_PrivilegesToCheck[rlPrivileges::PRIVILEGE_COMMUNICATIONS]);

						pBank->AddToggle("COMMUNICATION_VOICE_INGAME", &s_PrivilegesToCheck[rlPrivileges::PRIVILEGE_COMMUNICATION_VOICE_INGAME]);
						pBank->AddToggle("COMMUNICATION_VOICE_SKYPE", &s_PrivilegesToCheck[rlPrivileges::PRIVILEGE_COMMUNICATION_VOICE_SKYPE]);
						pBank->AddToggle("GAME_DVR", &s_PrivilegesToCheck[rlPrivileges::PRIVILEGE_GAME_DVR]);
						pBank->AddToggle("MULTIPLAYER_PARTIES", &s_PrivilegesToCheck[rlPrivileges::PRIVILEGE_MULTIPLAYER_PARTIES]);
						pBank->AddToggle("MULTIPLAYER_SESSIONS", &s_PrivilegesToCheck[rlPrivileges::PRIVILEGE_MULTIPLAYER_SESSIONS]);

						pBank->AddToggle("PREMIUM_CONTENT", &s_PrivilegesToCheck[rlPrivileges::PRIVILEGE_PREMIUM_CONTENT]);
						pBank->AddToggle("PREMIUM_VIDEO", &s_PrivilegesToCheck[rlPrivileges::PRIVILEGE_PREMIUM_VIDEO]);
						pBank->AddToggle("PROFILE_VIEWING", &s_PrivilegesToCheck[rlPrivileges::PRIVILEGE_PROFILE_VIEWING]);
						pBank->AddToggle("DOWNLOAD_FREE_CONTENT", &s_PrivilegesToCheck[rlPrivileges::PRIVILEGE_DOWNLOAD_FREE_CONTENT]);
						pBank->AddToggle("PURCHASE_CONTENT", &s_PrivilegesToCheck[rlPrivileges::PRIVILEGE_PURCHASE_CONTENT]);
						pBank->AddToggle("SHARE_KINECT_CONTENT", &s_PrivilegesToCheck[rlPrivileges::PRIVILEGE_SHARE_KINECT_CONTENT]);

						pBank->AddToggle("SOCIAL_NETWORK_SHARING", &s_PrivilegesToCheck[rlPrivileges::PRIVILEGE_SOCIAL_NETWORK_SHARING]);
						pBank->AddToggle("SUBSCRIPTION_CONTENT", &s_PrivilegesToCheck[rlPrivileges::PRIVILEGE_SUBSCRIPTION_CONTENT]);
						pBank->AddToggle("USER_CREATED_CONTENT", &s_PrivilegesToCheck[rlPrivileges::PRIVILEGE_USER_CREATED_CONTENT]);
						pBank->AddToggle("VIDEO_COMMUNICATIONS", &s_PrivilegesToCheck[rlPrivileges::PRIVILEGE_VIDEO_COMMUNICATIONS]);
						pBank->AddToggle("VIEW_FRIENDS_LIST", &s_PrivilegesToCheck[rlPrivileges::PRIVILEGE_VIEW_FRIENDS_LIST]);
#else
						pBank->AddTitle("This is unimplemented for this platform.");
#endif //RSG_DURANGO
					}
					pBank->PopGroup();

					pBank->AddSeparator("");

					pBank->AddButton("Is Privilege Check Result Ready", datCallback(NetworkBank_IsPrivilegeCheckResultReady));
					pBank->AddButton("Is Privilege Check In Progress", datCallback(NetworkBank_IsPrivilegeCheckInProgress));
					pBank->AddButton("Is Privilege Check Successful", datCallback(NetworkBank_IsPrivilegeCheckSuccessful));
					pBank->AddButton("Set Privilege Check Result Not Needed", datCallback(NetworkBank_SetPrivilegeCheckResultNotNeeded));
				}
				pBank->PopGroup(); // Privileges

				pBank->PushGroup("SystemUI");
				{
					pBank->AddText("Test Result:", &s_SystemUiDisplayString[0], MAX_SYSTEMUI_DISPLAY_STRING_LENGTH, true);
					pBank->AddSeparator("");

					pBank->AddButton("Is System UI Showing", datCallback(NetworkBank_IsSystemUiShowing));
					pBank->AddSeparator("");

					pBank->AddButton("Show Sign In UI", datCallback(NetworkBank_ShowSignInUi));
					pBank->AddButton("Show Party UI", datCallback(NetworkBank_ShowPartyUi));
					pBank->AddButton("Show Send Invite UI", datCallback(NetworkBank_ShowSendInviteUi));
                    pBank->AddButton("Show Account Upgrade UI", datCallback(NetworkBank_ShowAccountUpgradeUi));

					g_szFakeAppLaunchFlags[0] = '\0';

					pBank->AddSeparator("");
					pBank->AddButton("Fake Suspend Event", datCallback(NetworkBank_FakeSuspend));
					pBank->AddButton("Fake Suspend Immediate Event", datCallback(NetworkBank_FakeSuspendImmediate));
					pBank->AddButton("Fake Resume Event", datCallback(NetworkBank_FakeResume));
					pBank->AddText("App Launch Flags", g_szFakeAppLaunchFlags, sysService::MAX_ARG_LENGTH, false);
					pBank->AddButton("Fake App Launch Event", datCallback(NetworkBank_FakeAppLaunch));
#if RL_NP_SUPPORT_PLAY_TOGETHER
					pBank->AddButton("Fake Play Together Event", datCallback(Network_BankFakePlayTogether));	
#endif
					g_szBase64Text[0] = '\0';
					g_szBase64CovertedText[0] = '\0';

					pBank->AddSeparator("");
					pBank->AddText("Text", g_szBase64Text, sysService::MAX_ARG_LENGTH, false);
					pBank->AddText("Converted", g_szBase64CovertedText, sysService::MAX_ARG_LENGTH);
					pBank->AddButton("Encode Base64", datCallback(NetworkBank_EncodeBase64));
					pBank->AddButton("Decode Base64", datCallback(NetworkBank_DecodeBase64));
				}
				pBank->PopGroup(); // SystemUI

			    pBank->AddToggle("Socket Status", &sm_ToggleCallback, LiveBank_SpewSocketStatus);

			    pBank->PushGroup("Achievements");
			    {
				    pBank->AddSeparator();
				    for (unsigned i=0; i<MAX_ACHIEVEMENTS; i++)
				    {
					    pBank->AddSeparator();
					    pBank->AddText("Label", &s_AchLabels[i][0], ACHIEVEMENT_LABEL);
					    pBank->AddSlider("Id", &s_AchIds[i], INT_MIN, INT_MAX, 0);
					    pBank->AddToggle("Is Achieved", &s_AchIsAchieved[i]);
					    pBank->AddToggle("Set Achieved", &s_AwardAch[i], &LiveBank_AwardAch);
				    }
			    }
			    pBank->PopGroup();

				pBank->PushGroup("PC Only Achievements");
				{
					pBank->AddButton("Vinewood Visionary", &LiveBank_AwardRockstarEditorAch);
				}
				pBank->PopGroup();

#if __STEAM_BUILD
				pBank->PushGroup("Steam Achievements");
				{
					pBank->AddSeparator();
					pBank->AddSlider("Progress", &s_SteamAchProgress, 0, 100, 1);
					pBank->AddSlider("Ach Id", &s_SteamAchId, 0, player_schema::Achievements::COUNT, 1);
					pBank->AddButton("Get Progress", &LiveBank_GetSteamAchProgress);
					pBank->AddButton("Set Progress", &LiveBank_SetSteamAchProgress);
				}
				pBank->PopGroup();
#endif

				pBank->PushGroup("Youtube");
				{
					pBank->AddButton("Youtube OAuth", datCallback(NetworkBank_YoutubeOauth));
					pBank->AddButton("Show Youtube UI", datCallback(NetworkBank_ShowYoutubeUi));
				}
				pBank->PopGroup();

#if RSG_NP
				pBank->PushGroup("PremiumFeature", false);
				{
					pBank->AddToggle("Toggle Spectator Mode", &s_BankInSpectatorMode);
				}
				pBank->PopGroup();
#endif

				CNetworkNewsStoryMgr::Get().InitWidgets(pBank);
				CNetworkEmailMgr::Get().InitWidgets(pBank);

                sm_Commerce.InitWidgets( *pBank );
				sm_NetworkClanDataMgr.InitWidgets( *pBank );
#if RL_FACEBOOK_ENABLED
				sm_Facebook.InitWidgets( *pBank );
#endif // #if RL_FACEBOOK_ENABLED
				GamePresenceEvents::AddWidgets(*pBank);
				SC_INBOX_MGR.AddWidgets(*pBank);
				SocialClubEventMgr::Get().AddWidgets(*pBank);

				sm_ProfileStatsMgr.Bank_InitWidgets( *pBank );

				pBank->PushGroup("Profanity Filter");
					formatf(s_ProfanityString, "This string is fucking wrong");
					pBank->AddText("Check String", s_ProfanityString, netProfanityFilter::MAX_STRING_SIZE, false);
					pBank->AddButton("Do Profanity Check", datCallback(Bank_DoProfanifyCheck));
					pBank->AddButton("Get Profanity Check Status", datCallback(Bank_CheckProfanityToken));
				pBank->PopGroup();

				pBank->PushGroup("Social Network Privileges and Age");
					pBank->AddButton("Show sharing privileges", LiveBank_ShowSharingPrivileges);
					pBank->AddButton("Show Age Group", LiveBank_ShowAgeGroup);
				pBank->PopGroup();

				pBank->PushGroup("Game Region");
					pBank->AddButton("Show Game Region", LiveBank_ShowGameRegion);
				pBank->PopGroup();

#if GEN9_STANDALONE_ENABLED
                pBank->PushGroup("Social Club Feed");
                {
                    CSocialClubFeedMgr::Get().InitWidgets(*pBank);
                    CSocialClubFeedTilesMgr::Get().InitWidgets(*pBank);
                }
                pBank->PopGroup();
#endif
		    }
		    pBank->PopGroup();

#if RSG_ORBIS
			pBank->PushGroup("PS4 NP");
				pBank->AddButton("Send Invite UI", datCallback(PS4Bank_ShowPS4InviteSend));
				pBank->AddButton("Receive Invite UI", datCallback(PS4Bank_ShowPS4InviteReceive));
				pBank->AddButton("PS Plus Upsell", datCallback(PS4Bank_PSPlusUpsell));
				pBank->AddButton("Refresh Permissions", datCallback(PS4Bank_RefreshPermissions));
				pBank->AddButton("Check Presence Status", datCallback(PS4Bank_CheckPresenceConnection));
                pBank->AddButton("Convert Local Account Id to Online Id", datCallback(PS4Bank_AccountIdToOnlineId));
                pBank->AddButton("Convert Local Online Id to Account Id", datCallback(PS4Bank_OnlineIdToAccountId));
                pBank->AddButton("Show Profile From Account Id", datCallback(PS4Bank_ShowAccountIdGamerProfile));
			pBank->PopGroup();
#elif RSG_DURANGO
			pBank->PushGroup("XB1 User Events");
				pBank->AddSlider("Character Index", &s_CharacterIndex, 0, 2, 1);
				pBank->AddSlider("Stat Int", &s_StatInt, 0, 100, 5);
				pBank->AddSlider("Stat Float", &s_StatFloat, 0, 100, 5);
				pBank->AddButton("Write Cash Earned MP", datCallback(XB1Bank_WriteCashEarnedMP));
				pBank->AddButton("Write Enemy Defeated SP", datCallback(XB1Bank_EnemyDefeatedSP));
				pBank->AddButton("Write Game Progress SP", datCallback(XB1Bank_GameProgressSP));
				pBank->AddButton("Write Held Up Shops", datCallback(XB1Bank_HeldUpShops));
				pBank->AddButton("Write Horde Waves Survived", datCallback(XB1Bank_HordeWavesSurvived));
				pBank->AddButton("Write Money Total Spent", datCallback(XB1Bank_MoneyTotalSpent));
				pBank->AddButton("Write Multiplayer Round End", datCallback(XB1Bank_MultiplayerRoundEnd));
				pBank->AddButton("Write Multiplayer Round Start", datCallback(XB1Bank_MultiplayerRoundStart));
				pBank->AddButton("Write Platinum Awards",  datCallback(XB1Bank_PlatinumAwards));
				pBank->AddButton("Write Player Defeated SP", datCallback(XB1Bank_PlayerDefeatedSP));
				pBank->AddButton("Write Player Kills MP", datCallback(XB1Bank_PlayerKillsMP));
				pBank->AddButton("Write Player Session End", datCallback(XB1Bank_PlayerSessionEnd));
				pBank->AddButton("Write Player Session Pause", datCallback(XB1Bank_PlayerSessionPause));
				pBank->AddButton("Write Player Session Resume", datCallback(XB1Bank_PlayerSessionResume));
				pBank->AddButton("Write Player Session Start", datCallback(XB1Bank_PlayerSessionStart));
				pBank->AddButton("Write Player Respawned SP",datCallback(XB1Bank_PlayerRespawnedSP));
				pBank->AddButton("Write Total Custom Races Won", datCallback(XB1Bank_TotalCustomRacesWon));
				// Hero Stats
				pBank->AddButton("Write Game Progress SP", datCallback(XB1Bank_GameProgressSP));
				pBank->AddButton("Write Playing Time", datCallback(XB1Bank_PlayingTime));
				pBank->AddButton("Write Cash Earned SP", datCallback(XB1Bank_CashEarnedSP));
				pBank->AddButton("Write Stolen Cars", datCallback(XB1Bank_StolenCars));
				pBank->AddButton("Write Stars Attained", datCallback(XB1Bank_StarsAttained));
				pBank->AddButton("Write Multiplayer Play Time", datCallback(XB1Bank_MultiplayerPlaytime));
				pBank->AddButton("Write Cash Earned", datCallback(XB1Bank_CashEarnedMP));
				pBank->AddButton("Write Kill Death Ratio", datCallback(XB1Bank_KillDeathRatio));
				pBank->AddButton("Write Arch Enemy", datCallback(XB1Bank_ArchEnemy));
				pBank->AddButton("Write Highest Rank MP", datCallback(XB1Bank_HighestRankMP));
			pBank->PopGroup();
			pBank->PushGroup("XB1 Active Contexts");
				pBank->AddButton("Get Active Contexts", datCallback(XB1Bank_GetActiveContext));
				pBank->AddButton("Set SP Missions", datCallback(XB1Bank_ActiveContext1));
				pBank->AddButton("Set DeathmLoc", datCallback(XB1Bank_ActiveContext5));
				pBank->AddButton("Set MP Missions", datCallback(XB1Bank_ActiveContext7));
				pBank->AddButton("Set SP Minigame", datCallback(XB1Bank_ActiveContext8));
				pBank->AddButton("Set SP OffMission", datCallback(XB1Bank_ActiveContext9));
			pBank->PopGroup();
			pBank->PushGroup("XB1 Party");
				pBank->AddButton("Refresh Party", datCallback(XB1Bank_RefreshParty));
			pBank->PopGroup();
#endif

			pBank->PushGroup("Statistics", false);
			{
				pBank->AddSlider("Fly inverted check value", &CStatsMgr::minInvertFlightDot, -1, 0, 0.1f);
			}
			pBank->PopGroup();
            
            if(resetCurrentGroupAtEnd)
            {
                pBank->UnSetCurrentGroup(*debugGroup);
            }
        }
	}
}

void
CLiveManager::WidgetShutdown()
{
	if (s_BankGroup && BANKMGR.FindBank("Network"))
	{
		BANKMGR.FindBank("Network")->DeleteGroup(*s_GroupFriendsInvites);
		BANKMGR.FindBank("Network")->DeleteGroup(*s_BankGroup);
	}
	s_BankGroup = NULL;
}

void
CLiveManager::LiveBank_RefreshAchInfo()
{
	for (unsigned i=0; i<MAX_ACHIEVEMENTS; i++)
	{
		if (sm_AchMgr.m_AchievementsInfo[i].GetLabel() && sm_AchMgr.m_AchievementsInfo[i].GetId() > 0)
		{
			formatf(s_AchLabels[i], ACHIEVEMENT_LABEL, sm_AchMgr.m_AchievementsInfo[i].GetLabel());
			s_AchIds[i] = sm_AchMgr.m_AchievementsInfo[i].GetId();
			s_AchIsAchieved[i] = sm_AchMgr.HasAchievementBeenPassed(sm_AchMgr.m_AchievementsInfo[i].GetId());
		}
	}
}

void
CLiveManager::LiveBank_AwardAch()
{
	for (unsigned i=0; i<MAX_ACHIEVEMENTS; i++)
	{
		if (s_AwardAch[i]
			&& '\0' != s_AchLabels[i][0]
			&& !s_AchIsAchieved[i]
			&& s_AchIds[i] > 0)
		{
			sm_AchMgr.AwardAchievement(s_AchIds[i]);
			s_AwardAch[i] = false;
		}
	}
}

void
CLiveManager::LiveBank_AwardRockstarEditorAch()
{
	// Unlock ACH H11 - Vinewood Visionary
	CLiveManager::GetAchMgr().AwardAchievement(player_schema::Achievements::ACHH11);
}

#if __STEAM_BUILD
void CLiveManager::LiveBank_GetSteamAchProgress()
{
	sm_AchMgr.GetAchievementProgress(s_SteamAchId);
}
void CLiveManager::LiveBank_SetSteamAchProgress()
{
	sm_AchMgr.SetAchievementProgress(s_SteamAchId, s_SteamAchProgress);
}
#endif // __STEAM_BUILD

void
CLiveManager::LiveBank_Update()
{
	// add / remove the invite button depending on whether there is an active session or not
	if(s_GroupFriendsInvites)
	{
		if(NetworkInterface::IsGameInProgress())
		{
			if(!s_InviteButton)
			{
				s_InviteButton = s_GroupFriendsInvites->AddButton("Invite Friend", datCallback(NetworkBank_InviteFriend));
			}
		}
		else if(s_InviteButton)
		{
			s_InviteButton->Destroy();
			s_InviteButton = NULL;
		}
	}

    static unsigned lastTime = fwTimer::GetSystemTimeInMilliseconds();
           unsigned thisTime = fwTimer::GetSystemTimeInMilliseconds();

    if((thisTime > lastTime) && ((thisTime - lastTime) > 5000))
    {
        lastTime = thisTime;

        if (s_FriendCombo)
	    {
			// update the friend list
			s_NumFriends = sm_FriendsPage.m_NumFriends;
			for(u32 i = 0; i < s_NumFriends; i++)
			{
				s_Friends[i] = sm_FriendsPage.m_Friends[i].GetName();
			}

			// avoid empty combo boxes
			int nNumFriends = s_NumFriends;
			if(nNumFriends == 0)
			{
				s_Friends[0] = "--No Friends--";
				nNumFriends  = 1;
			}

		    s_FriendCombo->UpdateCombo("Friend", &s_FriendComboIndex, nNumFriends, (const char**)s_Friends);
	    }

	    if (s_InviteCombo)
	    {
			// update the invite list
			s_NumInvites = CLiveManager::GetInviteMgr().GetNumUnacceptedInvites();

			for(u32 i = 0; i < s_NumInvites; i++)
			{
				const UnacceptedInvite* pInvite = CLiveManager::GetInviteMgr().GetUnacceptedInvite(i);
				s_Invites[i] = pInvite->m_InviterName;
			}

			int nNumInvites = s_NumInvites;
			if(nNumInvites == 0)
			{
				s_Invites[0] = "--No Invites--";
				nNumInvites  = 1;
			}

		    s_InviteCombo->UpdateCombo("Invites", &s_InviteComboIndex, nNumInvites, (const char **)s_Invites);
	    }
    }
	
#if RSG_XBOX || RSG_PC
	if (s_WidgetFindGamerTag)
	{
		s_WidgetFindGamerTag = CLiveManager::GetFindGamerTag().Pending();
	}
#endif

#if RSG_DURANGO || RSG_ORBIS
	for(int i = (COUNTOF(s_WidgetDisplayNameRequestId) - 1); i >= 0; --i)
	{
		if(s_WidgetDisplayNameRequestId[i] != CDisplayNamesFromHandles::INVALID_REQUEST_ID)
		{
			rlDisplayName displayNames[CDisplayNamesFromHandles::MAX_DISPLAY_NAMES_PER_REQUEST] = {0};
			int result = CLiveManager::GetFindDisplayName().GetDisplayNames(s_WidgetDisplayNameRequestId[i], displayNames, CDisplayNamesFromHandles::MAX_DISPLAY_NAMES_PER_REQUEST);
			if(result != CDisplayNamesFromHandles::DISPLAY_NAMES_PENDING)
			{
				if(result == CDisplayNamesFromHandles::DISPLAY_NAMES_SUCCEEDED)
				{
					gnetDebug1("CDisplayNamesFromHandles succeeded:");
					for(unsigned i = 0; i < CDisplayNamesFromHandles::MAX_DISPLAY_NAMES_PER_REQUEST; ++i)
					{
						if(displayNames[i][0] != '\0')
						{
							gnetDebug1("%s", displayNames[i]);
						}
					}
				}
				else if(result == CDisplayNamesFromHandles::DISPLAY_NAMES_FAILED)
				{
					gnetDebug1("CDisplayNamesFromHandles failed");
				}

				s_WidgetDisplayNameRequestId[i] = CDisplayNamesFromHandles::INVALID_REQUEST_ID;
			}
			else
			{
				if(i == 3)
				{
					// simulate someone abandoning a request
					s_WidgetDisplayNameRequestId[i] = CDisplayNamesFromHandles::INVALID_REQUEST_ID;
				}
			}
		}
	}
#endif
}

void
CLiveManager::LiveBank_TestPresence()
{
	if(NetworkInterface::GetRichPresenceMgr().IsInitialized())
	{
#if RSG_NP
		NetworkInterface::GetRichPresenceMgr().SetPresenceStatus(s_PresenceId, s_PresenceStatus);
#endif
		sm_ToggleCallback = false;
	}
}

void
CLiveManager::LiveBank_TestPresenceExhaustion()
{
	if(NetworkInterface::GetRichPresenceMgr().IsInitialized())
	{
#if RSG_NP
		NetworkInterface::GetRichPresenceMgr().SetPresenceStatus(s_PresenceId, s_PresenceStatus);
		NetworkInterface::GetRichPresenceMgr().SetPresenceStatus(s_PresenceId, s_PresenceStatus);
		NetworkInterface::GetRichPresenceMgr().SetPresenceStatus(s_PresenceId, s_PresenceStatus);
		NetworkInterface::GetRichPresenceMgr().SetPresenceStatus(s_PresenceId, s_PresenceStatus);
		NetworkInterface::GetRichPresenceMgr().SetPresenceStatus(s_PresenceId, s_PresenceStatus);
		NetworkInterface::GetRichPresenceMgr().SetPresenceStatus(s_PresenceId, s_PresenceStatus);
#endif
		sm_ToggleCallback = false;
	}
}

void
CLiveManager::LiveBank_SpewSocketStatus()
{
	fiDeviceTcpIp::PrintInterfaceState();
	fiDeviceTcpIp::PrintNetStats();
	sm_ToggleCallback = false;
}

void
CLiveManager::LiveBank_ShowSharingPrivileges()
{
	bool bAllowedSocialNetSharingPrivilege = CLiveManager::GetSocialNetworkingSharingPrivileges();

	if(bAllowedSocialNetSharingPrivilege)
	{
		Displayf("\t Allowed social network sharing\n");
	}
	else
	{
		Displayf("\t Not allowed social network sharing \n");
	}
}

void
CLiveManager::LiveBank_ShowAgeGroup()
{
	rlAgeGroup ageGroup = CLiveManager::GetAgeGroup();

	switch(ageGroup)
	{
	case RL_AGEGROUP_CHILD:
		Displayf("\t RL_AGEGROUP_CHILD\n");
		break;
	case RL_AGEGROUP_ADULT:
		Displayf("\t RL_AGEGROUP_ADULT\n");
		break;
	case RL_AGEGROUP_TEEN:	
		Displayf("\t RL_AGEGROUP_TEEN\n");
		break;
	case RL_AGEGROUP_PENDING:
		Displayf("\t RL_AGEGROUP_PENDING\n");
		break;
	case RL_AGEGROUP_INVALID:
		Displayf("\t RL_AGEGROUP_INVALID\n");
		break;
	}
}

void
CLiveManager::LiveBank_ShowGameRegion()
{
	rlGameRegion gameRegion = rlGetGameRegion();

	switch(gameRegion)
	{
	case RL_GAME_REGION_EUROPE:
		Displayf("\t RL_GAME_REGION_EUROPE\n");
		break;
	case RL_GAME_REGION_AMERICA:    
		Displayf("\t RL_GAME_REGION_AMERICA\n");
		break;
	case RL_GAME_REGION_JAPAN:
		Displayf("\t RL_GAME_REGION_JAPAN\n");
		break;

	default:
		Displayf("\t Game region invalid %d \n",gameRegion);
		break; 
	}
}

#endif // __BANK

rlPlatformPartyMember::rlPlatformPartyMember()
{
	Clear();
}

rlPlatformPartyMember::~rlPlatformPartyMember()
{

}

void rlPlatformPartyMember::Clear()
{

}

rlGamerHandle* rlPlatformPartyMember::GetGamerHandle(rlGamerHandle* hGamer) const
{
	hGamer->Clear();

#if RSG_ORBIS
	rlSceNpOnlineId onlineId;
	onlineId.Reset(m_MemberInfo.m_SceNpPartyMemberInfo.npId.handle); 
	hGamer->ResetNp(g_rlNp.GetEnvironment(), RL_INVALID_NP_ACCOUNT_ID, &onlineId);
#elif RSG_XDK
	*hGamer = m_PartyInfo.m_GamerHandle;
#endif

	return hGamer;
}

const char* rlPlatformPartyMember::GetDisplayName() const
{
#if RSG_ORBIS
	return m_MemberInfo.m_SceNpPartyMemberInfo.npId.handle.data;
#elif RSG_XDK
	return m_PartyInfo.m_GamerName;
#else
	return "";
#endif
}

bool rlPlatformPartyMember::IsInSession() const
{
#if RSG_ORBIS
	return m_MemberInfo.m_IsInSession;
#elif RSG_XDK
	return m_PartyInfo.m_IsInSession;
#else
	return false;
#endif
}

rlSessionInfo* rlPlatformPartyMember::GetSessionInfo(rlSessionInfo* pInfo) const
{
	pInfo->Clear();

#if RSG_XDK
	*pInfo = m_PartyInfo.m_SessionInfo;
#endif

	return pInfo;
}

#if RSG_XBOX

void CGamerTagFromHandle::Clear()
{
    gnetDebug1("CGamerTagFromHandle::Clear");
    ZeroMemory(&m_Gamertag, sizeof(m_Gamertag));
	m_Status.Reset();
}

bool CGamerTagFromHandle::Start(const rlGamerHandle& handle)
{
	if(gnetVerify(!Pending()))
	{
		Clear();

		if(!gnetVerify(handle.IsValid()))
		{
            gnetError("CGamerTagFromHandle::Start :: Invalid handle!");
            return false;
		}

		m_GamerHandle = handle;

        gnetDebug1("CGamerTagFromHandle::Start :: Requested for %s", NetworkUtils::LogGamerHandle(m_GamerHandle));

		if(g_rlXbl.GetProfileManager()->GetPlayerNames(NetworkInterface::GetLocalGamerIndex(), &m_GamerHandle, 1, NULL, (rlGamertag*)m_Gamertag, &m_Status))
		{
            gnetError("CGamerTagFromHandle::Start :: GetPlayerNames failed!");
            return true;
		}
	}

	return false;
}

bool CGamerTagFromHandle::Pending()
{
	return m_Status.Pending();
}

void CGamerTagFromHandle::Cancel()
{
	if (m_Status.Pending())
	{
        gnetDebug1("CGamerTagFromHandle::Cancel");
        g_rlXbl.GetProfileManager()->CancelPlayerNameRequest(&m_Status);
	}
}

const char* CGamerTagFromHandle::GetGamerTag(const rlGamerHandle& handle)
{
    if(gnetVerify(handle.IsValid()))
    {
        gnetError("CGamerTagFromHandle::GetGamerTag :: Invalid handle!");
        return "";
    }

    if(gnetVerify(m_Status.Succeeded()))
    {
        gnetError("CGamerTagFromHandle::GetGamerTag :: Request has not succeeded!");
        return "";
    }
  
    if(gnetVerify(handle == m_GamerHandle))
    {
        gnetError("CGamerTagFromHandle::GetGamerTag :: Mismatched handles - Supplied: %s, Requested: %s", NetworkUtils::LogGamerHandle(handle), NetworkUtils::LogGamerHandle(m_GamerHandle));
        return "";
    }

	return m_Gamertag;
}

bool CGamerTagFromHandle::Succeeded()
{
	return m_Status.Succeeded();
}

#endif // RSG_XBOX

#if RSG_PC

// Note: on PC, we can only retrieve the gamertag from gamerhandles of local players, network session members, and friends
// We don't want to hit SCS services to get these names, so only locally available gamertags are supported.

void CGamerTagFromHandle::Clear()
{
	ZeroMemory(&m_Gamertag, sizeof(m_Gamertag));
	m_Status.Reset();
}

bool CGamerTagFromHandle::Start(const rlGamerHandle& handle)
{
	if(gnetVerify(!Pending()))
	{
		Clear();

		if(!gnetVerify(handle.IsValid()))
		{
			return false;
		}

		m_Gamertag[0] = '\0';
		m_GamerHandle = handle;

		bool found = false;

		// local players
		for(unsigned i = 0; i < RL_MAX_LOCAL_GAMERS; i++)
		{
			rlGamerHandle hGamer;
			rlPresence::GetGamerHandle(i, &hGamer);
			if(hGamer.IsValid() && (hGamer == m_GamerHandle))
			{
				rlPresence::GetName(i, m_Gamertag);
				found = true;
				break;
			}
		}
				
		// network session members
		if(!found && NetworkInterface::IsGameInProgress())
		{
			CNetGamePlayer* player = NetworkInterface::GetPlayerFromGamerHandle(m_GamerHandle);
			if(player)
			{
				safecpy(m_Gamertag, player->GetGamerInfo().GetName());
				found = true;
			}
		}

		// friends
		if(!found)
		{
			rlFriendsPage *friendPage = CLiveManager::GetFriendsPage();
			if(friendPage)
			{
				unsigned numFriends = friendPage->m_NumFriends;
				for(unsigned i = 0; i < numFriends; ++i)
				{
					rlGamerHandle hGamer;
					rlFriend& f = friendPage->m_Friends[i];
					if(f.IsValid())
					{
						f.GetGamerHandle(&hGamer);
						if(hGamer.IsValid() && (hGamer == m_GamerHandle))
						{
							safecpy(m_Gamertag, f.GetName());
							found = true;
							break;
						}
					}
				}
			}
		}

		m_Status.SetPending();

		if(found)
		{
			m_Status.SetSucceeded();
		}
		else
		{
			m_Status.SetFailed();
		}

		return gnetVerifyf(found, "Can only retrieve the gamertag from gamerhandles of local players, network session members, and friends on PC");
	}

	return false;
}

bool CGamerTagFromHandle::Pending()
{
	return m_Status.Pending();
}

void CGamerTagFromHandle::Cancel()
{
	if(m_Status.Pending())
	{
		
	}
}

const char* CGamerTagFromHandle::GetGamerTag(const rlGamerHandle& handle)
{
	if(gnetVerify(handle.IsValid() && m_Status.Succeeded() && handle == m_GamerHandle))
	{
		return m_Gamertag;
	}

	return "";
}

bool CGamerTagFromHandle::Succeeded()
{
	return m_Status.Succeeded();
}

#endif // RSG_PC

#if RSG_DURANGO || RSG_ORBIS

void CDisplayNamesFromHandles::CDisplayNameRequest::Clear()
{
	// NOTE: do not clear m_RequestId here. We always keep track of which request id was last used.
	m_Timestamp = 0;
	m_AbandonTime = 0;
	m_LocalGamerIndex = -1;
	m_NumGamerHandles = 0;
}

void CDisplayNamesFromHandles::Clear()
{
	m_CurrentRequestId = CDisplayNamesFromHandles::INVALID_REQUEST_ID;

	for(unsigned i = 0; i < MAX_QUEUED_REQUESTS; ++i)
	{
		m_Requests[i].Clear();
		m_Requests[i].m_RequestId = CDisplayNamesFromHandles::INVALID_REQUEST_ID;
	}
}

void CDisplayNamesFromHandles::CancelRequest(const int requestId)
{
	rtry
	{
		int requestIndex = GetRequestIndexByRequestId(requestId);

		rverify(requestIndex >= 0
			,catchall
			,gnetError("Failed CancelRequest - request id '%d' (request index '%d').", requestId, requestIndex));

		rverify(requestIndex < MAX_QUEUED_REQUESTS
			,catchall
			,gnetError("Failed CancelRequest requestIndex < MAX_QUEUED_REQUESTS - request id '%d' (request index '%d', MAX_QUEUED_REQUESTS='%d'.).", requestId, requestIndex, MAX_QUEUED_REQUESTS));


		m_Requests[requestIndex].Clear();

		if (m_CurrentRequestId == requestId)
		{
			gnetDebug3("Cancelled current CancelRequest with request id %d (request index %d)", requestId, requestIndex);

#if RSG_DURANGO
			if (m_Status.Pending())
			{
				g_rlXbl.GetProfileManager()->CancelPlayerNameRequest(&m_Status);
			}
#elif RSG_ORBIS
			if (m_Status.Pending())
			{
				netTask::Cancel(&m_Status);
			}
#endif

			sysMemSet(m_DisplayNames, 0, sizeof(m_DisplayNames));
			m_Status.Reset();
			m_CurrentRequestId = CDisplayNamesFromHandles::INVALID_REQUEST_ID;
		}
		else
		{
			gnetDebug3("Cancelled CDisplayNameRequest with request id %d (request index %d)", requestId, requestIndex);
		}
	}
	rcatchall
	{

	}
}

void CDisplayNamesFromHandles::CancelAbandonedRequests()
{
	for(unsigned i = 0; i < MAX_QUEUED_REQUESTS; ++i)
	{
		gnetAssert((m_Requests[i].m_RequestId == INVALID_REQUEST_ID) || GetRequestIndexByRequestId(m_Requests[i].m_RequestId) == (int)i);
		if(m_Requests[i].m_Timestamp != 0)
		{
			gnetAssert(m_Requests[i].m_NumGamerHandles > 0);
			u32 elapsed = sysTimer::GetSystemMsTime() - m_Requests[i].m_Timestamp;
			if(elapsed > m_Requests[i].m_AbandonTime )
			{
				gnetDebug3("CancelAbandonedRequests with request id %d (request index %d) has gone unqueried for %u ms. Cancelling...", m_Requests[i].m_RequestId, i, elapsed);
				CancelRequest(m_Requests[i].m_RequestId);
			}
		}
	}
}

void CDisplayNamesFromHandles::Update()
{
	CancelAbandonedRequests();
}

int CDisplayNamesFromHandles::GetRequestIndexByRequestId(const int requestId)
{
	if(requestId >= 0)
	{
		u32 index = requestId % MAX_QUEUED_REQUESTS;
		if(m_Requests[index].m_RequestId == requestId)
		{
			return index;
		}
	}

	return INVALID_REQUEST_ID;
}

int CDisplayNamesFromHandles::RequestDisplayNames(const int localGamerIndex, const rlGamerHandle* gamers, const unsigned numGamers, const u32 abandonTime)
{
	int requestId = CDisplayNamesFromHandles::INVALID_REQUEST_ID;

	rtry
	{
		rverify(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex), catchall, );
		rverify(gamers != NULL, catchall, );
		rverify(numGamers > 0, catchall, );
		rverify(numGamers <= MAX_DISPLAY_NAMES_PER_REQUEST, catchall, );

		// find an available request
		CDisplayNameRequest* request = NULL;
		for(unsigned i = 0; i < MAX_QUEUED_REQUESTS; ++i)
		{
			if(m_Requests[i].m_NumGamerHandles == 0)
			{
				// assign unique request id (never reused) that easily maps back to its index in the m_Requests array
				// 0	1	2	3	4	5	6	7	--> array index
				// ------------------------------
				// 0	1	2	3	4	5	6	7	--> request ids for each index
				// 8	9	10	11	12	13	14	15
				// 16	17	18	19	20	21	22	23
				// 24	25	26	27	28	29	30	31
				// ...

				request = &m_Requests[i];
				if(request->m_RequestId == CDisplayNamesFromHandles::INVALID_REQUEST_ID)
				{
					request->m_RequestId = i;
				}
				else
				{
					request->m_RequestId += MAX_QUEUED_REQUESTS;
				}
				requestId = request->m_RequestId;
				gnetDebug3("Assigned CDisplayNameRequest with request id %d (request index %d)", requestId, GetRequestIndexByRequestId(requestId));
				gnetDebug3("Requesting names for %u gamers", numGamers);

#if !__NO_OUTPUT
				char buf[RL_MAX_GAMER_HANDLE_CHARS];
				for (unsigned i = 0; i < numGamers; i++)
				{
					gnetDebug3("\t[%u] %s", i + 1, gamers[i].ToString(buf));
				}
#endif
				break;
			}
		}

		rcheck(request != NULL, catchall, );
		request->Clear();

		for(unsigned i = 0; i < numGamers; i++)
		{
			if(!gnetVerify(gamers[i].IsValid()))
			{
				request->Clear();
				return CDisplayNamesFromHandles::INVALID_REQUEST_ID;
			}

			request->m_GamerHandles[i] = gamers[i];
		}

		request->m_LocalGamerIndex = localGamerIndex;
		request->m_NumGamerHandles = numGamers;
		request->m_Timestamp = sysTimer::GetSystemMsTime();
		request->m_AbandonTime = abandonTime;
	}
	rcatchall
	{

	}

	return requestId;
}

int CDisplayNamesFromHandles::GetDisplayNames(const int requestId, rlDisplayName* displayNames, const unsigned maxDisplayNames)
{
	rtry
	{
		int requestIndex = GetRequestIndexByRequestId(requestId);

		rverify(requestIndex >= 0
				,catchall
				,gnetError("Failed GetRequestIndexByRequestId - request id '%d' (request index '%d').", requestId, requestIndex));

		rverify(requestIndex < MAX_QUEUED_REQUESTS
			,catchall
			,gnetError("Failed requestIndex < MAX_QUEUED_REQUESTS - request id '%d' (request index '%d', MAX_QUEUED_REQUESTS='%d'.).", requestId, requestIndex, MAX_QUEUED_REQUESTS));

		rverify(displayNames
			,catchall
			,gnetError("Failed displayNames - request id '%d' (request index '%d').", requestId, requestIndex));

		rverify(maxDisplayNames > 0
			,catchall
			,gnetError("Failed maxDisplayNames > 0 - request id '%d' (request index '%d', maxDisplayNames='%d').", requestId, requestIndex, maxDisplayNames));

		CDisplayNameRequest* request = &m_Requests[requestIndex];
		
		rverify(request->m_NumGamerHandles > 0
			,catchall
			,gnetError("Failed request->m_NumGamerHandles > 0 - request id '%d' (request index '%d', m_NumGamerHandles='%d').", requestId, requestIndex, request->m_NumGamerHandles));

		rverify(request->m_NumGamerHandles <= maxDisplayNames
				,catchall
				,gnetError("Failed request->m_NumGamerHandles (%u) <= maxDisplayNames (%u)- request id '%d' (request index '%d').", request->m_NumGamerHandles, maxDisplayNames, requestId, requestIndex));

		request->m_Timestamp = sysTimer::GetSystemMsTime();

		if(m_CurrentRequestId == CDisplayNamesFromHandles::INVALID_REQUEST_ID)
		{
			gnetDebug3("Setting current CDisplayNameRequest with request id %d (request index %d)", requestId, requestIndex);
			m_CurrentRequestId = requestId;
		}

		if(m_CurrentRequestId != requestId)
		{
			// we're processing a different request, requestId is queued
			return DISPLAY_NAMES_PENDING;
		}

		if(m_Status.Pending())
		{
			return DISPLAY_NAMES_PENDING;
		}
		else if(m_Status.Failed())
		{
			gnetDebug3("CDisplayNameRequest with request id %d (request index %d) has failed", requestId, requestIndex);

			// clear the request to make it available for reuse
			request->Clear();
			sysMemSet(m_DisplayNames, 0, sizeof(m_DisplayNames));
			m_Status.Reset();
			m_CurrentRequestId = CDisplayNamesFromHandles::INVALID_REQUEST_ID;
			return DISPLAY_NAMES_FAILED;
		}
		else if(m_Status.Succeeded())
		{
			gnetDebug3("CDisplayNameRequest with request id %d (request index %d) has succeeded", requestId, requestIndex);

			// copy names and clear the request to make it available for reuse
			for(unsigned i = 0; i < request->m_NumGamerHandles; i++)
			{
				safecpy(displayNames[i], m_DisplayNames[i], sizeof(rlDisplayName));
			}

			request->Clear();
			sysMemSet(m_DisplayNames, 0, sizeof(m_DisplayNames));
			m_Status.Reset();
			m_CurrentRequestId = CDisplayNamesFromHandles::INVALID_REQUEST_ID;
			return DISPLAY_NAMES_SUCCEEDED;
		}

#if RSG_ORBIS
		if(g_rlNp.GetWebAPI().GetPlayerNames(request->m_LocalGamerIndex, request->m_GamerHandles, request->m_NumGamerHandles, m_DisplayNames, nullptr, rlNpGetProfilesWorkItem::FLAGS_DEFAULT, &m_Status))
		{
			return DISPLAY_NAMES_PENDING;
		}
#elif RSG_DURANGO
		// kick off a new async request
		sysMemSet(m_DisplayNames, 0, sizeof(m_DisplayNames));
		if(g_rlXbl.GetProfileManager()->GetPlayerNames(request->m_LocalGamerIndex, request->m_GamerHandles, request->m_NumGamerHandles, m_DisplayNames, NULL, &m_Status))
		{
			return DISPLAY_NAMES_PENDING;
		}
#endif
		else
		{
			request->Clear();
			sysMemSet(m_DisplayNames, 0, sizeof(m_DisplayNames));
			m_Status.Reset();
			m_CurrentRequestId = CDisplayNamesFromHandles::INVALID_REQUEST_ID;
			return DISPLAY_NAMES_FAILED;
		}
	}
	rcatchall
	{

	}

	return DISPLAY_NAMES_FAILED;
}
#endif

void CLiveManager::GetFriendsStats(unsigned& numFriendsTotal, unsigned& numFriendsInSameTitle, unsigned& numFriendsInMySession)
{
	numFriendsTotal = rlFriendsManager::GetTotalNumFriends(NetworkInterface::GetLocalGamerIndex());
	numFriendsInSameTitle = 0;
	numFriendsInMySession = 0;

	rlGamerHandle handler;

	for(int i = 0; i < numFriendsTotal; ++i)
	{
		if(rlFriendsManager::IsFriendInSameTitle(i))
		{
			++numFriendsInSameTitle;

			if (rlFriendsManager::IsFriendInSession(i))
			{
				rlGamerHandle gh;
				rlFriendsManager::GetGamerHandle(i, &gh);
				if (NetworkInterface::GetPlayerFromGamerHandle(gh) != NULL)
				{
					++numFriendsInMySession;
				}
			}
		}
	}
}

bool CLiveManager::RequestNewFriendsPage(int startIndex, int numFriends)
{
	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if(!IsSignedIn() || !IsOnline() || !HasValidRosCredentials(localGamerIndex))
	{
		return false;
	}

	if (RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex) &&
		!sm_FriendStatus.Pending() && rlVerify(numFriends <= sm_FriendsReadBuf.m_MaxFriends))
	{
		sm_FriendsReadBuf.ClearFriendsArray();
		sm_FriendsReadBuf.m_StartIndex = startIndex;

		gnetDebug3("Requesting new friends page: %d - %d", startIndex, startIndex + numFriends);
		int friendFlags = rlFriendsReader::FRIENDS_ALL | rlFriendsReader::FRIENDS_PRESORT_ONLINE | rlFriendsReader::FRIENDS_PRESORT_ID;
		rlFriendsManager::GetFriends(localGamerIndex, &sm_FriendsReadBuf, friendFlags, &sm_FriendStatus);
		return true;
	}

	return false;
}

#if RSG_NP
namespace
{
    enum class RealTimeMultiplayerDisableReason
    {
        NoDisableReason,
		OnLoadingScreen,
		InPlayerSwitch,
		CameraFaded,
		NoPlayer,
		NoPlayerControl,
		NotInOnlineMultiplayer,
        SessionNotEstablished,
        SoloMultiplayer,
        AloneInClosed,
        AloneInPrivate,
		AloneNotVisible,
		InIntro,
		ScriptDisable,
        Max
    };

#if !__NO_OUTPUT
    const char* RealTimeMultiplayerDisableReasonToString(RealTimeMultiplayerDisableReason reason)
    {
        using Reason = RealTimeMultiplayerDisableReason;

        const char* reasonStrings[] = {
			"NoDisableReason",
			"OnLoadingScreen",
			"InPlayerSwitch",
			"CameraFaded",
			"NoPlayer",
			"NoPlayerControl",
            "NotInOnlineMultiplayer",
            "SessionNotEstablished",
            "SoloMultiplayer",
            "AloneInClosed",
			"AloneInPrivate",
			"AloneNotVisible",
			"InIntro",
			"ScriptDisable",
        };

        static_assert(COUNTOF(reasonStrings) == (int)Reason::Max, "Enum and string array mismatch!");

        if((reason < Reason::NoDisableReason) || (reason >= Reason::Max))
            return "Invalid";

        return reasonStrings[(int)reason];
    };
#endif

#if RSG_PROSPERO
	unsigned GetPlayerControlThresholdTimeMs()
	{
		static const int DEFAULT_THRESHOLD_MS = 2000;
		int thresholdMs = DEFAULT_THRESHOLD_MS;

		if(Tunables::IsInstantiated())
			thresholdMs = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NET_REALTIME_MULTIPLAYER_CONTROL_THRESHOLD_MS", 0xca878930), DEFAULT_THRESHOLD_MS);

		return static_cast<unsigned>(thresholdMs);
	}

	bool ArePlayerControlsDisabled()
	{
		CPed* playerPed = FindPlayerPed();
		if(!playerPed)
			return false;

		CPlayerInfo* playerInfo = playerPed->GetPlayerInfo();
		if(!playerInfo)
			return false;

		// check if script have disabled our controls
		return playerInfo->AreAnyControlsOtherThanFrontendDisabled();
	}

	bool IsUsingScriptControlledWeapon()
	{
		CPed* playerPed = FindPlayerPed();
		if(!playerPed)
			return false;

		const CPedWeaponManager* weaponManager = playerPed->GetWeaponManager();
		if(weaponManager == nullptr)
			return false;

		const CWeapon* equippedWeapon = weaponManager->GetEquippedWeapon();
		if(equippedWeapon == nullptr)
			return false;

		const unsigned weaponHash = equippedWeapon->GetWeaponHash();

		return (weaponHash == ATSTRINGHASH("WEAPON_AIR_DEFENCE_GUN", 0x2c082d7d)) ||
			(weaponHash == ATSTRINGHASH("WEAPON_TRANQUILIZER", 0x32a888bd)) ||
			(weaponHash == ATSTRINGHASH("WEAPON_ARENA_HOMING_MISSILE", 0x648a81d0)) ||
			(weaponHash == ATSTRINGHASH("WEAPON_ARENA_MACHINE_GUN", 0x34fdff66));
	}

	bool IsScriptRunning(const unsigned nameHash)
	{
		const strLocalIndex idx = g_StreamedScripts.FindSlotFromHashKey(nameHash);
		if(idx.IsValid())
		{
			// catch the situation where the script isn't in memory
			if(g_StreamedScripts.GetNumRefs(idx) > 0)
			{
				scrProgram* pProgram = scrProgram::GetProgram(g_StreamedScripts.GetProgramId(idx.Get()));
				if(pProgram)
				{
					return (pProgram->GetNumRefs() - 1) > 0;
				}
			}
		}
		return false;
	}

	bool IsScriptRunningWithCustomControls()
	{
		return IsScriptRunning(ATSTRINGHASH("turret_cam_Script", 0xe6511181));
	}

	bool CanPlayerBeDamaged()
	{
		// if we can still be damaged, then we can consider them to be in gameplay
		// only consider that we can be damaged when script have set the flag (to avoid any default state counting)
		CPlayerInfo* playerInfo = CGameWorld::GetMainPlayerInfoSafe();
		if(playerInfo != nullptr)
			return playerInfo->GetPlayerDataCanBeDamaged() && playerInfo->IsPlayerDataDamageAllowedFromSetPlayerControl();

		return false;
	}

	bool HasPlayerControl()
	{
		// track this to allow us to avoid thrashing if the control is enabled and then disabled immediately
		static unsigned s_TimeDetectedPlayerControl = 0;

		// check if script have disabled our controls
		const bool controlsDisabled = ArePlayerControlsDisabled();

		// set to this but check if we have any exclusions to this rule
		bool hasPlayerControl = !controlsDisabled;
		if(!hasPlayerControl)
		{
			// check spectator mode override (script can override this)
			const bool isSpectatorMode = NetworkInterface::IsInSpectatorMode() &&
				!CLiveManager::HasRealtimeMultiplayerScriptFlags(RealTimeMultiplayerScriptFlags::RTS_SpectatorOverride);

			// script can disable no player control (in scenarios where we are still in gameplay
			// but script have a specific setup
			const bool hasScriptOverride = CLiveManager::HasRealtimeMultiplayerScriptFlags(RealTimeMultiplayerScriptFlags::RTS_PlayerOverride);

			// if we can be damaged, consider this gameplay
			const bool isPlayerDamagedEnabled = CanPlayerBeDamaged();

			hasPlayerControl =
				isPlayerDamagedEnabled ||
				hasScriptOverride ||
				isSpectatorMode ||						// Sony indicated that spectator modes still count
				IsUsingScriptControlledWeapon() ||		// This means controls are disabled but we are using a script controlled weapon
				IsScriptRunningWithCustomControls();	// Similar to above but checks an entire script
		}

		// track when we initially detect player controls
		if(hasPlayerControl && s_TimeDetectedPlayerControl == 0)
			s_TimeDetectedPlayerControl = sysTimer::GetSystemMsTime();
		else if(!hasPlayerControl)
			s_TimeDetectedPlayerControl = 0;

		// check we have control (use a minimum threshold value to prevent single frame toggles)
		return
			(s_TimeDetectedPlayerControl > 0) &&
			((sysTimer::GetSystemMsTime() - s_TimeDetectedPlayerControl) > GetPlayerControlThresholdTimeMs());
	}
#endif

    RealTimeMultiplayerDisableReason GetRealTimeMultiplayerDisableReason()
    {
        using Reason = RealTimeMultiplayerDisableReason;
	
#if RSG_PROSPERO
        // make sure we are in a public session
		if(!NetworkInterface::IsInAnyMultiplayer())
            return Reason::NotInOnlineMultiplayer;
        if(!NetworkInterface::IsSessionEstablished())
            return Reason::SessionNotEstablished;
        if(NetworkInterface::IsInSoloMultiplayer())
            return Reason::SoloMultiplayer;

		// in sessions alone that aren't publically joinable
        if(NetworkInterface::GetNumPhysicalPlayers() == 1)
        {
            // if we're in a session on our own that cannot be joined without an invite, block realtime
            if(NetworkInterface::IsInPrivateSession())
                return Reason::AloneInPrivate;
			if(NetworkInterface::IsInClosedSession())
				return Reason::AloneInClosed;
			if(NetworkInterface::AreAllSessionsNonVisible())
				return Reason::AloneNotVisible;
        }

		// if our active character has not completed the intro...
		if(NetworkInterface::GetActiveCharacterIndex() >= 0 && !NetworkInterface::HasCompletedMultiplayerIntro())
			return Reason::InIntro;

		// loading / switch transitions 
		if(NetworkInterface::IsOnLoadingScreen())
			return Reason::OnLoadingScreen;
		if(NetworkInterface::IsInPlayerSwitch())
			return Reason::InPlayerSwitch;
		if(camInterface::IsFadedOut() || camInterface::IsFadingOut())
			return Reason::CameraFaded;

		// check for player ped
		CPed* pPlayerPed = FindPlayerPed();
		if(!pPlayerPed || !pPlayerPed->GetPlayerInfo())
			return Reason::NoPlayer;

		if(!HasPlayerControl())
			return Reason::NoPlayerControl;
#elif RSG_ORBIS
		// for PS4, keep the existing checks for now
		if(!NetworkInterface::IsAnySessionActive())
			return Reason::NotInOnlineMultiplayer;
		if(NetworkInterface::IsInSoloMultiplayer())
			return Reason::SoloMultiplayer;
#endif

		// check if script have disabled
		if(CLiveManager::HasRealtimeMultiplayerScriptFlags(RealTimeMultiplayerScriptFlags::RTS_Disable))
			return Reason::ScriptDisable;

        return Reason::NoDisableReason;
    }
}

bool CLiveManager::HasRealtimeMultiplayerScriptFlags(const unsigned scriptFlag)
{
	return (sm_RealtimeMultiplayerScriptFlags & scriptFlag) == scriptFlag;
}

bool CLiveManager::IsInRealTimeMultiplayer()
{
    using Reason = RealTimeMultiplayerDisableReason;

    const Reason reason = GetRealTimeMultiplayerDisableReason();

#if !__NO_OUTPUT
    static Reason s_LastReason = Reason::NoDisableReason;
    if(s_LastReason != reason)
    {
        netRealtimeScriptDebug(
            "IsInRealTimeMultiplayer :: %s, Reason: %s",
            reason == Reason::NoDisableReason ? "True" : "False",
            RealTimeMultiplayerDisableReasonToString(reason));
    }
    s_LastReason = reason;
#endif

    return reason == Reason::NoDisableReason;
}
#endif // RSG_NP

void CLiveManager::UpdateMultiplayerTracking()
{
#define NET_SUPPORT_PLAY_TOGETHER_IN_LIVE (RL_NP_SUPPORT_PLAY_TOGETHER && 0)

    // online multiplayer
#if !__NO_OUTPUT
    static bool s_bWasInMultiplayer = false;
#endif
#if !__NO_OUTPUT || NET_SUPPORT_PLAY_TOGETHER_IN_LIVE
    const bool bIsInMultiplayer = NetworkInterface::IsSessionEstablished();
#endif
#if !__NO_OUTPUT
    // track these states
    if(bIsInMultiplayer != s_bWasInMultiplayer)
        gnetDebug1("UpdateMultiplayerTracking :: %s Online / Established Multiplayer", bIsInMultiplayer ? "Entering" : "Exiting");
#endif

#if NET_SUPPORT_PLAY_TOGETHER_IN_LIVE
    if(sm_PendingPlayTogetherGroup.m_NumPlayers > 0 && bIsInMultiplayer)
    {
        gnetDebug1("UpdateMultiplayerTracking :: PlayTogether - Sending Invites to %u players", sm_PendingPlayTogetherGroup.m_NumPlayers);
        sm_InviteMgr.SendPlayTogetherInvites(sm_PendingPlayTogetherGroup.m_Players, sm_PendingPlayTogetherGroup.m_NumPlayers);
        sm_PendingPlayTogetherGroup.m_NumPlayers = 0;
    }
#endif

#if !__NO_OUTPUT
    // update tracked state
    s_bWasInMultiplayer = bIsInMultiplayer;
#endif

#if !__NO_OUTPUT
	static eNetworkAccessCode s_AccessCode = eNetworkAccessCode::Access_Invalid;
	eNetworkAccessCode accessCode = CNetwork::CheckNetworkAccess(eNetworkAccessArea::AccessArea_MultiplayerEnter);

	// track these states
	if(accessCode != s_AccessCode)
	{
		gnetDebug1("UpdateMultiplayerTracking :: NetworkAccess: %d -> %d", s_AccessCode, accessCode);
		s_AccessCode = accessCode;
	}
#endif

#if RSG_NP
    // real-time multiplayer - this is used to track when we call 
    {
        // tracking transitions into / out of real-time multiplayer
#if !__NO_OUTPUT
		static bool s_bWasInRealtimeMultiplayer = false;
		static bool s_WasInSpectator = false;
		static bool s_DidCallNotifyPremiumFeature = false;
#endif

		// capture relevant states
        const bool bIsInRealtimeMultiplayer = IsInRealTimeMultiplayer();
		const bool isInSpectator = NetworkInterface::IsInSpectatorMode() BANK_ONLY(|| s_BankInSpectatorMode);

#if !__NO_OUTPUT
		// this is only needed for logging
		bool hasCalledNotifyPremiumFeature = false;

        // track these states
        if(bIsInRealtimeMultiplayer != s_bWasInRealtimeMultiplayer)
            netRealtimeScriptDebug("UpdateMultiplayerTracking :: %s Realtime Multiplayer", bIsInRealtimeMultiplayer ? "Entering" : "Exiting");
#endif

		// apply relevant properties call NotifyPremiumFeature
		unsigned properties = rlRealtimeMultiplayerProperty::Property_None;
		if(bIsInRealtimeMultiplayer)
        {
#if !__NO_OUTPUT
			hasCalledNotifyPremiumFeature = true;
#endif
			// build list of properties
			if(isInSpectator)
				properties |= rlRealtimeMultiplayerProperty::Property_InEngineSpectating;

            g_rlNp.GetNpAuth().NotifyPremiumFeature(NetworkInterface::GetLocalGamerIndex(), properties);
        }

#if !__NO_OUTPUT
        // track these states
        if(hasCalledNotifyPremiumFeature != s_DidCallNotifyPremiumFeature)
            netRealtimeScriptDebug("UpdateMultiplayerTracking :: %s calling NotifyPremiumFeature, Properties: 0x%x", hasCalledNotifyPremiumFeature ? "Started" : "Stopped", properties);

		// flag changes to spectator settings when already calling NotifyPremiumFeature or calling it for the first time and in spectator mode
		if((hasCalledNotifyPremiumFeature && isInSpectator != s_WasInSpectator) || (hasCalledNotifyPremiumFeature && !s_DidCallNotifyPremiumFeature && isInSpectator))
			netRealtimeScriptDebug("UpdateMultiplayerTracking :: %s applying spectator property for NotifyPremiumFeature", isInSpectator ? "Started" : "Stopped");

        // update tracked states
		s_DidCallNotifyPremiumFeature = hasCalledNotifyPremiumFeature;
        s_bWasInRealtimeMultiplayer = bIsInRealtimeMultiplayer;
		s_WasInSpectator = isInSpectator;
#endif
    }
#endif // RSG_NP

#if RSG_OUTPUT

#define TRACK_STATE_CHANGE_COMMON(logFn, track, fn, ctx, alwaysLogInitial) \
	static bool s_##track = false; \
	static bool s_Logged##track = false; \
	const bool current##track = (fn); \
	if(current##track != s_##track || (!s_Logged##track && alwaysLogInitial)) \
	{ \
		logFn("UpdateMultiplayerTracking :: %s: %s -> %s, Context: %s", #track, s_##track ? "True" : "False", current##track ? "True" : "False", ctx); \
	} \
	s_##track = current##track; \
	s_Logged##track = true;

#define TRACK_STATE_CHANGE_ALWAYS_LOG_INITIAL(logFn, track, fn, ctx) TRACK_STATE_CHANGE_COMMON(logFn, track, fn, ctx, true)
#define TRACK_STATE_CHANGE(logFn, track, fn, ctx) TRACK_STATE_CHANGE_COMMON(logFn, track, fn, ctx, false)

#define TRACK_VALUE_CHANGE_COMMON(logFn, track, fn, ctx, alwaysLogInitial) \
	static int s_##track = 0; \
	static bool s_Logged##track = false; \
	const int current##track = (fn); \
	if(current##track != s_##track || (!s_Logged##track && alwaysLogInitial)) \
	{ \
		logFn("UpdateMultiplayerTracking :: %s: %d -> %d, Context: %s", #track, s_##track, current##track, ctx); \
	} \
	s_##track = current##track; \
	s_Logged##track = true;

#define TRACK_VALUE_CHANGE_ALWAYS_LOG_INITIAL(logFn, track, fn, ctx) TRACK_VALUE_CHANGE_COMMON(logFn, track, fn, ctx, true)
#define TRACK_VALUE_CHANGE(logFn, track, fn, ctx) TRACK_VALUE_CHANGE_COMMON(logFn, track, fn, ctx, false)

	// track multiplayer state changes
	TRACK_STATE_CHANGE(gnetDebug1, IsInAnyMultiplayer, NetworkInterface::IsInAnyMultiplayer(), "None");
	GEN9_LANDING_PAGE_ONLY(TRACK_STATE_CHANGE(gnetDebug1, ScriptRouterLink, ScriptRouterLink::HasPendingScriptRouterLink(), ScriptRouterLink::GetScriptRouterLink().c_str()));

	// UI changes
	TRACK_STATE_CHANGE_ALWAYS_LOG_INITIAL(gnetDebug1, IsOnLoadingScreen, NetworkInterface::IsOnLoadingScreen(), "None");
	TRACK_STATE_CHANGE_ALWAYS_LOG_INITIAL(gnetDebug1, IsOnLandingPage, NetworkInterface::IsOnLandingPage(), "None");
	TRACK_STATE_CHANGE_ALWAYS_LOG_INITIAL(gnetDebug1, IsOnInitialInteractiveScreen, NetworkInterface::IsOnInitialInteractiveScreen(), "None");

	// track SP prologue
	TRACK_STATE_CHANGE_ALWAYS_LOG_INITIAL(gnetDebug1, HasCompletedSpPrologue, CNetwork::HasCompletedSpPrologue() == IntroSettingResult::Result_Complete, "None");

	// track stats changes
	TRACK_STATE_CHANGE_ALWAYS_LOG_INITIAL(gnetDebug1, IsReadyToCheckLandingPageStats, NetworkInterface::IsReadyToCheckLandingPageStats(), "None");
	TRACK_STATE_CHANGE_ALWAYS_LOG_INITIAL(gnetDebug1, IsReadyToCheckProfileStats, NetworkInterface::IsReadyToCheckProfileStats(), "None");
	TRACK_STATE_CHANGE_ALWAYS_LOG_INITIAL(gnetDebug1, HasActiveCharacter, NetworkInterface::HasActiveCharacter(), "None");
	TRACK_STATE_CHANGE_ALWAYS_LOG_INITIAL(gnetDebug1, HasCompletedMultiplayerIntro, NetworkInterface::HasCompletedMultiplayerIntro(), "None");
	TRACK_VALUE_CHANGE_ALWAYS_LOG_INITIAL(gnetDebug1, GetActiveCharacterIndex, NetworkInterface::GetActiveCharacterIndex(), "None");

	// track realtime changes
#if RSG_PROSPERO
	if(NetworkInterface::IsSessionEstablished())
	{
		TRACK_STATE_CHANGE_ALWAYS_LOG_INITIAL(netRealtimeScriptDebug, HasPlayerControl, HasPlayerControl(), "None");
		TRACK_STATE_CHANGE_ALWAYS_LOG_INITIAL(netRealtimeScriptDebug, ArePlayerControlsDisabled, ArePlayerControlsDisabled(), "None");
		TRACK_STATE_CHANGE_ALWAYS_LOG_INITIAL(netRealtimeScriptDebug, IsInSpectatorMode, NetworkInterface::IsInSpectatorMode(), "None");
		TRACK_STATE_CHANGE_ALWAYS_LOG_INITIAL(netRealtimeScriptDebug, IsUsingScriptControlledWeapon, IsUsingScriptControlledWeapon(), "None");
		TRACK_STATE_CHANGE_ALWAYS_LOG_INITIAL(netRealtimeScriptDebug, IsScriptRunningWithCustomControls, IsScriptRunningWithCustomControls(), "None");
		TRACK_STATE_CHANGE_ALWAYS_LOG_INITIAL(netRealtimeScriptDebug, CanPlayerBeDamaged, CanPlayerBeDamaged(), "None");
		TRACK_VALUE_CHANGE_ALWAYS_LOG_INITIAL(netRealtimeScriptDebug, RealtimeMultiplayerScriptFlags, sm_RealtimeMultiplayerScriptFlags, "None");
		TRACK_STATE_CHANGE_ALWAYS_LOG_INITIAL(netRealtimeScriptDebug, IsInPlayerSwitch, NetworkInterface::IsInPlayerSwitch(), "None");
		TRACK_STATE_CHANGE_ALWAYS_LOG_INITIAL(netRealtimeScriptDebug, IsFadedOut, gVpMan.PrimaryOrthoViewportExists() && (camInterface::IsFadedOut() || camInterface::IsFadingOut()), "None");
	}
#endif

	// track ped
	TRACK_STATE_CHANGE(gnetDebug1, HasPed, FindPlayerPed() != nullptr, "None");
#endif
}

void CLiveManager::OnTunablesRead()
{
	SCE_ONLY(sm_bFlagIpReleasedOnGameResume = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("LM_FLAG_IP_RELEASE_ON_GAME_RESUME", 0xcaf958dd), true));
	sm_LargeCrewThreshold = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NET_LARGE_CREW_THRESHOLD", 0x20a2280e), DEFAULT_LARGE_CREW_THRESHOLD);
	MEMBERSHIP_ONLY(sm_EvaluateBenefitsInterval = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("NET_EVALUATE_BENEFITS_INTERVAL", 0x60ac0f21), DEFAULT_EVALUATE_BENEFITS_INTERVAL));

	sm_InviteMgr.OnTunablesRead();
}

#if RSG_NP
#if !__NO_OUTPUT
struct RealtimeMultiplayerTracker
{
	struct RealtimeMultiplayerScript
	{
		unsigned m_ScriptHash;
		char m_ScriptName[64];
		unsigned m_ScriptFlags;
	};

	bool RegisterRealtimeMultiplayerScript(const unsigned scriptFlags)
	{
		unsigned scriptHash = CTheScripts::GetCurrentScriptHash();

		// check for this hash in our existing 
		int numThisFrameScripts = m_ScriptsDisablingThisFrame.GetCount();
		for(int i = 0; i < numThisFrameScripts; i++)
		{
			if(m_ScriptsDisablingThisFrame[i].m_ScriptHash == scriptHash)
			{
				// just |= the flags
				m_ScriptsDisablingThisFrame[i].m_ScriptFlags |= scriptFlags;
				return false;
			}
		}

		if(m_ScriptsDisablingThisFrame.IsFull())
		{
			netRealtimeScriptDebug("RegisterRealtimeMultiplayerScript :: No space to add new scripts!");
			return false;
		}

		// register new script for this frame
		RealtimeMultiplayerScript& disablingScript = m_ScriptsDisablingThisFrame.Append();
		disablingScript.m_ScriptHash = scriptHash;
		safecpy(disablingScript.m_ScriptName, CTheScripts::GetCurrentScriptName());
		disablingScript.m_ScriptFlags = scriptFlags;

		return true;
	}

	void UpdateAfterScripts()
	{
		// log all scripts which were there last frame and are no longer there this frame
		int numLastFrameScripts = m_ScriptsDisablingLastFrame.GetCount();
		int numThisFrameScripts = m_ScriptsDisablingThisFrame.GetCount();
		int numExpiredScripts = 0;

		// loop round all current frame scripts and check if any are new
		for(int i = 0; i < numThisFrameScripts; i++)
		{
			bool hadScript = false;
			unsigned previousFlags = 0;
			for(int j = 0; j < numLastFrameScripts; j++)
			{
				if(m_ScriptsDisablingLastFrame[j].m_ScriptHash == m_ScriptsDisablingThisFrame[i].m_ScriptHash)
				{
					// was called this frame and last frame
					hadScript = true;
					previousFlags = m_ScriptsDisablingLastFrame[j].m_ScriptFlags;
					break;
				}
			}

			if(!hadScript)
			{
				netRealtimeScriptDebug("UpdateAfterScripts :: New script: %s [0x%08x], Flags: 0x%x, Total Scripts: %d",
					m_ScriptsDisablingThisFrame[i].m_ScriptName,
					m_ScriptsDisablingThisFrame[i].m_ScriptHash,
					m_ScriptsDisablingThisFrame[i].m_ScriptFlags,
					m_ScriptsDisablingThisFrame.GetCount());
			}
			else if(hadScript && (previousFlags != m_ScriptsDisablingThisFrame[i].m_ScriptFlags))
			{
				netRealtimeScriptDebug("UpdateAfterScripts :: Changed Flags: %s [0x%08x], Flags: 0x%x > 0x%x",
					m_ScriptsDisablingThisFrame[i].m_ScriptName,
					m_ScriptsDisablingThisFrame[i].m_ScriptHash,
					previousFlags,
					m_ScriptsDisablingThisFrame[i].m_ScriptFlags);
			}
		}

		// loop round all last frame scripts to check if any have expired
		for(int i = 0; i < numLastFrameScripts; i++)
		{
			bool hasScript = false;
			for(int j = 0; j < numThisFrameScripts; j++)
			{
				if(m_ScriptsDisablingThisFrame[j].m_ScriptHash == m_ScriptsDisablingLastFrame[i].m_ScriptHash)
				{
					// was called this frame and last frame
					hasScript = true;
					break;
				}
			}

			if(!hasScript)
			{
				numExpiredScripts++;
				netRealtimeScriptDebug("UpdateAfterScripts :: Expired script: %s [0x%08x], Total Scripts: %d",
					m_ScriptsDisablingLastFrame[i].m_ScriptName,
					m_ScriptsDisablingLastFrame[i].m_ScriptHash,
					m_ScriptsDisablingLastFrame.GetCount() - numExpiredScripts);
			}
		}

		m_ScriptsDisablingLastFrame = m_ScriptsDisablingThisFrame;
		m_ScriptsDisablingThisFrame.Reset();
	}

	atFixedArray<RealtimeMultiplayerScript, 32> m_ScriptsDisablingThisFrame;
	atFixedArray<RealtimeMultiplayerScript, 32> m_ScriptsDisablingLastFrame;
};
static RealtimeMultiplayerTracker s_RealtimeScriptTracker;
#endif

void CLiveManager::SetRealtimeMultiplayerScriptFlags(const unsigned scriptFlags)
{
#if !__NO_OUTPUT
	s_RealtimeScriptTracker.RegisterRealtimeMultiplayerScript(scriptFlags);

	// log each new setting (not set last frame, not currently set this frame but provided in this call)
	if((sm_PreviousRealtimeMultiplayerScriptFlags & RealTimeMultiplayerScriptFlags::RTS_Disable) == 0 && (sm_RealtimeMultiplayerScriptFlags & RealTimeMultiplayerScriptFlags::RTS_Disable) == 0 && (scriptFlags & RealTimeMultiplayerScriptFlags::RTS_Disable) != 0)
		netRealtimeScriptDebug("SetRealtimeMultiplayerScriptFlags :: Added: RTS_Disable");
	if((sm_PreviousRealtimeMultiplayerScriptFlags & RealTimeMultiplayerScriptFlags::RTS_PlayerOverride) == 0 && (sm_RealtimeMultiplayerScriptFlags & RealTimeMultiplayerScriptFlags::RTS_PlayerOverride) == 0 && (scriptFlags & RealTimeMultiplayerScriptFlags::RTS_PlayerOverride) != 0)
		netRealtimeScriptDebug("SetRealtimeMultiplayerScriptFlags :: Added: RTS_PlayerOverride");
	if((sm_PreviousRealtimeMultiplayerScriptFlags & RealTimeMultiplayerScriptFlags::RTS_SpectatorOverride) == 0 && (sm_RealtimeMultiplayerScriptFlags & RealTimeMultiplayerScriptFlags::RTS_SpectatorOverride) == 0 && (scriptFlags & RealTimeMultiplayerScriptFlags::RTS_SpectatorOverride) != 0)
		netRealtimeScriptDebug("SetRealtimeMultiplayerScriptFlags :: Added: RTS_SpectatorOverride");
#endif

	sm_RealtimeMultiplayerScriptFlags |= scriptFlags;
}
#endif

void CLiveManager::UpdateBeforeScripts()
{
#if RSG_NP
	sm_RealtimeMultiplayerScriptFlags = RealTimeMultiplayerScriptFlags::RTS_None;
#endif
}

void CLiveManager::UpdateAfterScripts()
{
	PROFILE;

#if RSG_NP
#if !__NO_OUTPUT
	// log out all scripts that have stopped disabling
	s_RealtimeScriptTracker.UpdateAfterScripts();

	// log each expiring setting (previously set, not currently set)
	if((sm_PreviousRealtimeMultiplayerScriptFlags & RealTimeMultiplayerScriptFlags::RTS_Disable) != 0 && (sm_RealtimeMultiplayerScriptFlags & RealTimeMultiplayerScriptFlags::RTS_Disable) == 0)
		netRealtimeScriptDebug("UpdateAfterScripts :: Expired: RTS_Disable");
	if((sm_PreviousRealtimeMultiplayerScriptFlags & RealTimeMultiplayerScriptFlags::RTS_PlayerOverride) != 0 && (sm_RealtimeMultiplayerScriptFlags & RealTimeMultiplayerScriptFlags::RTS_PlayerOverride) == 0)
		netRealtimeScriptDebug("UpdateAfterScripts :: Expired: RTS_PlayerOverride");
	if((sm_PreviousRealtimeMultiplayerScriptFlags & RealTimeMultiplayerScriptFlags::RTS_SpectatorOverride) != 0 && (sm_RealtimeMultiplayerScriptFlags & RealTimeMultiplayerScriptFlags::RTS_SpectatorOverride) == 0)
		netRealtimeScriptDebug("UpdateAfterScripts :: Expired: RTS_SpectatorOverride");

	// cache flags for comparisons next frame
	sm_PreviousRealtimeMultiplayerScriptFlags = sm_RealtimeMultiplayerScriptFlags;
#endif
#endif
}

// eof

