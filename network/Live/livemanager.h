//
// livemanager.h
//
// Copyright (C) 1999-2009 Rockstar Games. All Rights Reserved.
//

#ifndef CLIVEMANAGER_H
#define CLIVEMANAGER_H

// --- Include Files ------------------------------------------------------------
#if RSG_XBOX
#include "rline/durango/rlxblparty_interface.h"
#include "rline/durango/rlxbluserstatistics_interface.h"
#include "rline/durango/rlXblPrivacySettings_interface.h"
#endif

#include "rline/rl.h"
#include "rline/rlgamerinfo.h"
#include "rline/rlpresence.h"
#include "rline/presence/rlpresencequery.h"
#include "rline/scmembership/rlscmembership.h"
#include "system/service.h"

#include "fwnet/netscgamerdatamgr.h"

#include "Core/Game.h"
#include "network/commerce/CommerceManager.h"
#include "network/crews/NetworkCrewDataMgr.h"
#include "network/Facebook/Facebook.h"
#include "network/Live/AchMgr.h"
#include "network/Live/InviteMgr.h"
#include "network/Live/SocialClubMgr.h"
#include "network/Party/NetworkPartyInvites.h"

#define ENABLE_SINGLE_PLAYER_CLOUD_SAVES (RSG_PC && 1)
#if ENABLE_SINGLE_PLAYER_CLOUD_SAVES
#include "rline/rlpc.h"
#endif

#if __WIN32PC && __STEAM_BUILD
#pragma warning(disable: 4265)
#include "../../3rdParty/Steam/public/steam/steam_api.h"
#pragma warning(error: 4265)
#endif

// --- Forward Definitions ------------------------------------------------------
namespace rage
{
	class sysMemSimpleAllocator;
	class richPresenceMgr;
	class rlMarketplace;

#if __BANK
	class bkGroup;
#endif // __BANK
}

class NetworkClan;
class CProfileStats;

#if COMMERCE_CONTAINER
#include "system/timer.h"

#define IF_COMMERCE_STORE_RETURN()					\
if (CLiveManager::GetCommerceMgr().ContainerIsStoreMode())	\
	return;
#define IF_COMMERCE_STORE_RETURN_TRUE()				\
if (CLiveManager::GetCommerceMgr().ContainerIsStoreMode())	\
	return true;
#define IF_COMMERCE_STORE_RETURN_FALSE()			\
if (CLiveManager::GetCommerceMgr().ContainerIsStoreMode())	\
	return false;
#else
#define IF_COMMERCE_STORE_RETURN()
#define IF_COMMERCE_STORE_RETURN_TRUE()
#define IF_COMMERCE_STORE_RETURN_FALSE()
#endif

class rlPlatformPartyMember
{
	friend class CLiveManager;

public:

	rlPlatformPartyMember();

	~rlPlatformPartyMember();

	void Clear();

	//PURPOSE
	//  Retrieves the gamer handle for the party member.
	rlGamerHandle* GetGamerHandle(rlGamerHandle* hGamer) const;
	
	//PURPOSE
	//  Returns the party members display name
	const char* GetDisplayName() const;

	//PURPOSE
	//  Returns if this player is in a network session
	bool IsInSession() const;

	//PURPOSE
	//  Retrieves the session info for a 
	rlSessionInfo* GetSessionInfo(rlSessionInfo* pInfo) const; 

private:

#if RSG_ORBIS
	rlNpPartyMemberInfo m_MemberInfo;
#endif

#if RSG_DURANGO
	rlXblPartyMemberInfo m_PartyInfo;
#endif
};

#if RSG_XBOX
class CGamerTagFromHandle
{
public:
	CGamerTagFromHandle() { Clear(); }
	~CGamerTagFromHandle() 
	{

	}

	void  Cancel();
	bool  Pending();
	bool  Succeeded();

	bool              Start(const rlGamerHandle& handle);
	const char* GetGamerTag(const rlGamerHandle& handle);

private:
	void  Clear();

private:

	rlGamerHandle		  m_GamerHandle;
	netStatus             m_Status;
	char				  m_Gamertag[RL_MAX_NAME_BUF_SIZE];
};
#endif

#if RSG_PC
class CGamerTagFromHandle
{
public:
	CGamerTagFromHandle() { Clear(); }
	~CGamerTagFromHandle() 
	{

	}

	void  Cancel();
	bool  Pending();
	bool  Succeeded();

	bool              Start(const rlGamerHandle& handle);
	const char* GetGamerTag(const rlGamerHandle& handle);

private:
	void  Clear();

private:

	rlGamerHandle		  m_GamerHandle;
	netStatus             m_Status;
	char				  m_Gamertag[RL_MAX_NAME_BUF_SIZE];
};
#endif

#if RSG_DURANGO || RSG_ORBIS

class CDisplayNamesFromHandles
{
public:
	static const unsigned MAX_QUEUED_REQUESTS = 8;
	static const unsigned MAX_DISPLAY_NAMES_PER_REQUEST = 100; // to accomodate rlFriendsPage::MAX_FRIEND_PAGE_SIZE
	static const int INVALID_REQUEST_ID = -1;
	static const u32 ABANDONED_TIMEOUT_MS = 15 * 1000;	// requests that go unqueried for this long will be canceled

	enum
	{
		DISPLAY_NAMES_FAILED = -1,
		DISPLAY_NAMES_SUCCEEDED = 0,
		DISPLAY_NAMES_PENDING = 1,
	};

	CDisplayNamesFromHandles() {Clear();}
	~CDisplayNamesFromHandles() 
	{
	}

	// PURPOSE
	// Cancels a pending request using the given request Id.
	void CancelRequest(const int requestId);

	//PURPOSE
	// Queues up a request to get a list display names.
	// Returns a requestId which is passed to GetDisplayNames (see below).
	// If the returned value is < 0, an error has occured and the requestId is not valid.
	int RequestDisplayNames(const int localGamerIndex, const rlGamerHandle* gamers, const unsigned numGamers, const u32 abandonTime = ABANDONED_TIMEOUT_MS);

	//PURPOSE
	// Takes a requestId issued by RequestDisplayNames.
	// Returns an error code (succeeded, pending, failed). Upon succeeding, displayNames is populated.
	// If pending is returned, then the request is still retrieving display names.
	// The requestId is no longer valid after this function returns succeeded or failed
	// (the request is considered completed and is added back to the request pool so it can be reused).
	int GetDisplayNames(const int requestId, rlDisplayName* displayNames, const unsigned maxDisplayNames);

	//PURPOSE
	// Updates display name requests.
	void Update();

	//PURPOSE
	// Returns the index of a request by its request id.
	// Will return -1 if the request id is invalid or it was an old request id.
	int GetRequestIndexByRequestId(const int requestId);

private:
	void CancelAbandonedRequests();
	void Clear();

private:

	class CDisplayNameRequest
	{
		friend class CDisplayNamesFromHandles;
	public:
		CDisplayNameRequest()
		{
			CompileTimeAssert(MAX_DISPLAY_NAMES_PER_REQUEST <= (1 << (sizeof(m_NumGamerHandles) << 3)));
			m_RequestId = INVALID_REQUEST_ID;
			Clear();
		}

		~CDisplayNameRequest() 
		{
			
		}

	private:
		void Clear();

	private:
		rlGamerHandle m_GamerHandles[MAX_DISPLAY_NAMES_PER_REQUEST];
		u32 m_Timestamp;
		u32 m_AbandonTime;
		int m_LocalGamerIndex;
		u8 m_NumGamerHandles;
		int m_RequestId; // DO NOT CLEAR in Clear();
	};

	CDisplayNameRequest m_Requests[MAX_QUEUED_REQUESTS];
	rlDisplayName m_DisplayNames[MAX_DISPLAY_NAMES_PER_REQUEST];

	netStatus m_Status;
	int m_CurrentRequestId;
};
#endif // RSG_DURANGO

//PURPOSE
// High level interface to manage live stuff: Invites, Achievements, TUS, etc.
class CLiveManager
{
public:

	static const unsigned RLINE_HEAP_SIZE;
	static const unsigned MAX_FIND_GAMERS = 32;
	static const unsigned MAX_GAMER_STATUS = 100;
	static const int FRIEND_CACHE_PAGE_SIZE = 96;

private:

	//Invites
	static InviteMgr sm_InviteMgr;
	static PartyInviteMgr sm_PartyInviteMgr;
	static netSocketManager* sm_SocketMgr;

	// Rline Heap
	static sysMemAllocator *sm_RlineAllocator;

#if RSG_DURANGO
	// profile handing for Xbox to support cycling during PLM events
	static rlGamerHandle sm_SignedOutHandle;
    static bool sm_SignedInIntoNewProfileAfterSuspendMode;
#endif

    static bool sm_bOfflineInvitePending;
	static bool sm_bSubscriptionTelemetryPending;
	static bool sm_bMembershipTelemetryPending;
	static bool sm_bInitialisedSession;

	// Ros
	static bool sm_bIsOnlineRos[RL_MAX_LOCAL_GAMERS];
	static bool sm_bHasValidCredentials[RL_MAX_LOCAL_GAMERS];
	static bool sm_bHasValidRockstarId[RL_MAX_LOCAL_GAMERS];
    static bool sm_bHasCredentialsResult[RL_MAX_LOCAL_GAMERS];

	// PlayStation Plus
	static unsigned sm_OnlinePrivilegePromotionEndedTime[RL_MAX_LOCAL_GAMERS];

    // Privilege
	static bool sm_bHasSocialNetSharePriv[RL_MAX_LOCAL_GAMERS];
    static bool sm_bConsiderForPrivileges[RL_MAX_LOCAL_GAMERS];
	static bool sm_bPermissionsValid[RL_MAX_LOCAL_GAMERS];
    static bool sm_bHasOnlinePrivileges[RL_MAX_LOCAL_GAMERS];
	static bool sm_bIsRestrictedByAge[RL_MAX_LOCAL_GAMERS];
    static bool sm_bHasUserContentPrivileges[RL_MAX_LOCAL_GAMERS];
	static bool sm_bHasVoiceCommunicationPrivileges[RL_MAX_LOCAL_GAMERS];
	static bool sm_bHasTextCommunicationPrivileges[RL_MAX_LOCAL_GAMERS];
		
#if RSG_ORBIS
	static bool sm_bFlagIpReleasedOnGameResume;
#endif

	static int sm_LargeCrewThreshold;

#if RSG_XBOX
	static bool sm_bPendingPrivilegeCheckForUi;
	static rlPrivileges::PrivilegeTypes sm_PendingPrivilegeType;
	static int sm_PendingPrivilegeLocalGamerIndex;
#endif

	// Presence
	static rlPresence::Delegate sm_PresenceDlgt;
	static rlRos::Delegate sm_RosDlgt;
	static rlFriendsManager::Delegate sm_FriendDlgt;

#if NET_ENABLE_MEMBERSHIP_FUNCTIONALITY
	static rlScMembership::Delegate sm_MembershipDlgt;
#endif

	//Manage Rich Presence Messages
	static richPresenceMgr sm_RichPresenceMgr;

	//Number of friends in the match.
	static bool sm_bEvaluateFriendPeds;
	static rlFriendsPage sm_FriendsPage;
	static rlFriendsPage sm_FriendsReadBuf;
	static bool sm_bFriendActivatedNeedsUpdate;
	static netStatus sm_FriendStatus;
	static u32 sm_FriendsInMatch;

	// Manage Achievements information
	static AchMgr sm_AchMgr;

	//Set to TRUE after initialization
	static bool sm_IsInitialized;

	//Holds the last update time
	static unsigned sm_LastUpdateTime;

	//Local Player Profile stats
	static CProfileStats sm_ProfileStatsMgr;

	// Social Club Manager
	static SocialClubMgr sm_SocialClubMgr;

	// Clan Data Manager
	static CNetworkCrewDataMgr sm_NetworkClanDataMgr;

#if RL_FACEBOOK_ENABLED
	static CFacebook sm_Facebook;
#endif // #if RL_FACEBOOK_ENABLED
	
	static netSCGamerDataMgr sm_scGamerDataMgr;

	// status to track add friend calls
	static netStatus sm_AddFriendStatus;

	// Commerce Manager
	static cCommerceManager sm_Commerce;

	// for finding gamers via presence
	class FindGamerTask
	{
	public:

		netStatus m_Status;
		unsigned m_nGamers;
		rlGamerQueryData m_GamerData[MAX_FIND_GAMERS];
	};
	static FindGamerTask* sm_FindGamerTask;

	// for queuing gamers to query
	class GetGamerStatusQueue
	{
	public:

		GetGamerStatusQueue() : m_nGamers(0) {}
		unsigned m_nGamers;
		rlGamerHandle m_GamerQueue[MAX_GAMER_STATUS];
	};
	static GetGamerStatusQueue* sm_GetGamerStateQueue;

	// for getting gamer status
	class GetGamerStatusTask
	{
	public:
		
		netStatus m_Status;
		unsigned m_nGamers;
		eGamerState m_GamerStates[MAX_GAMER_STATUS];
		rlGamerHandle m_hGamers[MAX_GAMER_STATUS];
	};
	static GetGamerStatusTask* sm_GetGamerStatusTask;

#if RSG_XBOX || RSG_PC
	// Find GamerTag from UserId
	static CGamerTagFromHandle sm_FindGamerTag;
#endif

#if RSG_DURANGO || RSG_ORBIS
	// Find DisplayName from UserId
	static CDisplayNamesFromHandles sm_FindDisplayName;
#endif // RSG_DURANGO || RSG_ORBIS

#if RSG_XDK
	// Reputation handling
	static bool sm_OverallReputationIsBadNeedsRefreshed;
	static bool sm_OverallReputationIsBadRefreshing;
	static bool sm_OverallReputationIsBad;
	static rlXblUserStatiticsInfo sm_OverallReputationIsBadStatistic;
	static netStatus sm_StatisticsStatus; 
#endif // RSG_XDK

#if RSG_XBOX
	static PlayerList sm_AvoidList;
	static bool sm_bAvoidListNeedsRefreshed;
	static bool sm_bAvoidListRefreshing;
	static bool sm_bHasAvoidList;
	static netStatus sm_AvoidListStatus; 
#endif // RSG_XBOX

#if RSG_ORBIS
	static bool sm_bPendingAppLaunchScriptEvent;
	static unsigned sm_nPendingAppLaunchFlags;
#endif

#if ENABLE_SINGLE_PLAYER_CLOUD_SAVES
	static rgsc::ICloudSaveManifest* m_CloudSaveManifest;
	static rgsc::ICloudSaveManifestV2* m_CloudSaveManifestV2;
#endif

	static bool sm_bSuspendActionPending;
	static bool sm_bResumeActionPending;
	static bool sm_bGameWasSuspended; 

public:

	//PURPOSE
	// Initialize/Shutdown the needed Live stuff.
	static bool Init(const unsigned initMode);
	static void Shutdown(const unsigned shutdownMode);

	//PURPOSE
	// Return true if everything was well initialized.
	static bool IsInitialized() { return sm_IsInitialized; }

	//PURPOSE
	// Update Live services.
#if COMMERCE_CONTAINER
	// EJ: A hack that allows us to directly invoke live manager update without first going through network
	static void CommerceUpdate();
#endif

	static void Update(const unsigned curTime);

	//PURPOSE
	// Called before / after script updates
	static void UpdateBeforeScripts();
	static void UpdateAfterScripts();
	
	//PURPOSE
	// Respond to active controller changes
	static void InitProfile();
	static void ShutdownProfile();

	//PURPOSE
	// Fast access to local / main gamer info
	// PURPOSE
	//	Queries for presence and ros privileges per gamer index, managed by LiveManager but exposed to game code via NetworkInterface
	static bool IsGuest(int nGamerIndex = GAMER_INDEX_LOCAL);
	static bool IsSignedIn(int nGamerIndex = GAMER_INDEX_LOCAL);
	static bool IsOnline(int nGamerIndex = GAMER_INDEX_LOCAL);
	static bool HasRosCredentialsResult(int nGamerIndex = GAMER_INDEX_LOCAL);
	static bool IsOnlineRos(int nGamerIndex = GAMER_INDEX_LOCAL);
	static bool HasValidRosCredentials(int nGamerIndex = GAMER_INDEX_LOCAL);
	static bool HasCredentialsResult(int nGamerIndex = GAMER_INDEX_LOCAL);
	static bool IsRefreshingRosCredentials(int localGamerIndex = GAMER_INDEX_LOCAL);
	static bool HasValidRockstarId(int nGamerIndex = GAMER_INDEX_LOCAL);

	static bool IsPermissionsValid(const int nGamerIndex = GAMER_INDEX_LOCAL, const bool bCheckHasPrivilege = true);
    static bool CheckOnlinePrivileges(const int nGamerIndex = GAMER_INDEX_LOCAL, const bool bCheckHasPrivilege = true);
    static bool CheckUserContentPrivileges(const int nGamerIndex = GAMER_INDEX_LOCAL, const bool bCheckHasPrivilege = true);
    static bool CheckVoiceCommunicationPrivileges(const int nGamerIndex = GAMER_INDEX_LOCAL, const bool bCheckHasPrivilege = true);
	static bool CheckTextCommunicationPrivileges(const int nGamerIndex = GAMER_INDEX_LOCAL, const bool bCheckHasPrivilege = true);
	static bool CheckIsAgeRestricted(const int nGamerIndex = GAMER_INDEX_LOCAL);
	static bool GetSocialNetworkingSharingPrivileges(const int nGamerIndex = GAMER_INDEX_LOCAL);
	static rlAgeGroup GetAgeGroup();

	static bool HasRosPrivilege(const rlRosPrivilegeId id, const int nGamerIndex = GAMER_INDEX_LOCAL);
	
	static bool HadOnlinePermissionsPromotionWithinMs(const unsigned nWithinMs, int nGamerIndex = GAMER_INDEX_LOCAL);

#if RSG_XBOX
	static bool ResolvePlatformPrivilege(const int localGamerIndex, const rlPrivileges::PrivilegeTypes privilegeType, const bool waitForSystemUi);
	static int StartPermissionsCheck(IPrivacySettings::ePermissions nPermission, const rlGamerHandle& hGamer, const int nGamerIndex = GAMER_INDEX_LOCAL);
#endif

	// generic checks to setup tunables for and check whether a system can receive from it's allowed groups
	static unsigned GetValidSentFromGroupsFromTunables(const unsigned defaultGroups, const char* tunableStub);
	static bool CanReceiveFrom(const rlGamerHandle& sender, rlClanId crewId, const unsigned validSentFromGroups OUTPUT_ONLY(, const char* logStub));

	//PURPOSE
	// Presents the gamer sign-in UI.
	static bool ShowSigninUi(unsigned nSignInFlags = 0);

	//PURPOSE
	// Presents the gamer profile UI.
	//PARAMS
	// target - gamer for which you want to show the gamer profile.
	static bool ShowGamerProfileUi(const rlGamerHandle& target);

	//PURPOSE
	// Presents the gamer feedback UI.
	//PARAMS
	// target - gamer for which you give feedback.
	static void ShowGamerFeedbackUi(const rlGamerHandle& target);

	//PURPOSE
	// Presents the message compose UI. 
	//PARAMS
	// target - gamers that we are sending to
	static void ShowMessageComposeUI(const rlGamerHandle* pRecipients, unsigned nRecipients, const char* szSubject, const char* szMessage);

	//PURPOSE
	// Checks if we have platform subscription (PlayStation Plus, Xbox Live Gold)
	static bool HasPlatformSubscription(const bool initialCheckOnly = true);

	//PURPOSE
	// Checks if we have platform subscription pending
	static bool IsPlatformSubscriptionCheckPending(const bool initialCheckOnly = true);

	//PURPOSE
	// Checks if account is a sub account
	static bool IsSubAccount();

	//PURPOSE
	// Checks if account has signed out of online service
	static bool IsAccountSignedOutOfOnlineService();

	//PURPOSE
	// Presents the dialog required for account upgrades (Orbis-only for now).
	static bool ShowAccountUpgradeUI(const unsigned nDeepLinkBrowserCloseTime = 0);

	//PURPOSE
	//  Shows the app menu.
	static void ShowAppHelpUI();

	//PURPOSE
	// Platform party methods
	static bool ShowPlatformPartyUi();
	static bool SendPlatformPartyInvites();
	static bool IsInPlatformParty(); 
	static int GetPlatformPartyMemberCount();
	static unsigned GetPlatformPartyInfo(rlPlatformPartyMember* pMembers, unsigned nMembers);
	static bool JoinPlatformPartyMemberSession(const rlGamerHandle& hGamer); 
    static bool IsInPlatformPartyChat();
    static bool IsChattingInPlatformParty(const rlGamerHandle& hGamer);
    static bool ShowPlatformVoiceChannelUI(bool bVoiceRequired); 

	//PURPOSE
	// Called when a gamer joins the match.
	static void AddPlayer(const rlGamerInfo& gamerInfo);

	//PURPOSE
	// Called when a gamer exits the match.
	static void RemovePlayer(const rlGamerInfo& gamerInfo);

	//PURPOSE
	// Refresh the friend count in match
	static void RefreshFriendCountInMatch();

	//PURPOSE
	// Sends an invitation to a player to become your friend.
	static bool IsAddingFriend();
	static bool AddFriend(const rlGamerHandle& gamerHandle, const char* szMessage);
	static bool OpenEmptyFriendPrompt();

	//PURPOSE
	// Returns true if a system UI is currently showing.
	static bool IsSystemUiShowing();

	//PURPOSE
	// Returns true an deep link upsell has recently been requested
	static bool HasRecentUpsellRequest(const unsigned recentThresholdMs = 0);

    //PURPOSE
    // Access to offline invite pending flag
    static bool IsOfflineInvitePending();
    static void ClearOfflineInvitePending();
    static bool HasBootInviteToProcess();
    static bool ShouldSkipBootUi(OUTPUT_ONLY(bool logresult = false));
    static bool SignInInviteeForBoot();

	//PURPOSE
	// Find gamers presence query
	static bool FindGamersViaPresence(const char* szQuery, const char* pParams);
	static void ClearFoundGamers();
	static bool IsFindingGamers();
	static bool DidFindGamersSucceed(); 
	static unsigned GetNumFoundGamers();
	static bool GetFoundGamer(int nIndex, rlGamerQueryData* pQueryData); 
	static rlGamerQueryData* GetFoundGamers(unsigned* nNumGamers);
	
	//PURPOSE
	// Get gamer status 
	static bool GetGamerStatus(rlGamerHandle* pGamers, unsigned nGamers);
	static bool QueueGamerForStatus(const rlGamerHandle& hGamer);
	static bool GetGamerStatusFromQueue();
	static void ClearGetGamerStatus();

	static bool IsGettingGamerStatus();
	static bool DidGetGamerStatusSucceed();

	static unsigned GetNumGamerStatusGamers();
	static eGamerState* GetGamerStatusStates();
	static eGamerState GetGamerStatusState(int nIndex);
	static rlGamerHandle* GetGamerStatusHandles();
	static bool GetGamerStatusHandle(int nIndex, rlGamerHandle* phGamer);

	//PURPOSE
	//	Friend's page (subset of entire friends list)
	static rlFriendsPage* GetFriendsPage() { return &sm_FriendsPage; }
	static void GetFriendsStats(unsigned& numFriendsTotal, unsigned& numFriendsInSameTitle, unsigned& numFriendsInMySession);
	static bool RequestNewFriendsPage(int startIndex, int numFriends);
	static bool IsReadingFriends() { return sm_FriendStatus.Pending() || rlFriendsManager::IsReading(); }

	// for invites to a different user
	static void HandleInviteToDifferentUser(int nUser);

	// To throw the user back to the IIS if needed. This is a failsafe mechanism only to be called on the landing page
	static void CheckForSignOut();

	// --- Access Service Managers ----------------------------------------------------------------
	static netSocketManager*			GetSocketMgr();
	static InviteMgr&					GetInviteMgr()			{ return sm_InviteMgr; }
	static PartyInviteMgr&				GetPartyInviteMgr()		{ return sm_PartyInviteMgr; }
	static AchMgr&						GetAchMgr()				{ return sm_AchMgr; }
	static richPresenceMgr&				GetRichPresenceMgr()	{ return sm_RichPresenceMgr; }
	static SocialClubMgr&				GetSocialClubMgr()		{ return sm_SocialClubMgr; }
	static NetworkClan&					GetNetworkClan();
	static CNetworkCrewDataMgr&			GetCrewDataMgr()		{ return sm_NetworkClanDataMgr; }
	static CProfileStats&				GetProfileStatsMgr()	{ return sm_ProfileStatsMgr; }
#if RL_FACEBOOK_ENABLED
	static CFacebook&					GetFacebookMgr()		{ return sm_Facebook; }
#endif // #if RL_FACEBOOK_ENABLED
	static cCommerceManager&			GetCommerceMgr()		{ return sm_Commerce; }
	static netSCGamerDataMgr&			GetSCGamerDataMgr()		{ return sm_scGamerDataMgr; }
	static sysMemAllocator*				GetRlineAllocator()		{ return sm_RlineAllocator; } 

#if RSG_XBOX || RSG_PC
	static CGamerTagFromHandle&		    GetFindGamerTag()		{ return sm_FindGamerTag; } 
#endif

#if RSG_DURANGO || RSG_ORBIS
	static CDisplayNamesFromHandles&	GetFindDisplayName()	{ return sm_FindDisplayName; }
#endif

	static bool							IsOverallReputationBad()
	{ 
#if RSG_XDK
		return sm_OverallReputationIsBad; 
#else
		return false;
#endif
	}	

#if RSG_XBOX
	static bool							IsInAvoidList(const rlGamerHandle& hGamer); 
#endif

	static bool							IsInBlockList(const rlGamerHandle& hGamer); 

#if RSG_XBOX
	static bool							SignedInIntoNewProfileAfterSuspendMode() { return sm_SignedInIntoNewProfileAfterSuspendMode; }
	static void							ClearSignedInIntoNewProfileAfterSuspendMode();
#endif
	static bool							WasGameSuspended() { return sm_bGameWasSuspended; }

#if ENABLE_SINGLE_PLAYER_CLOUD_SAVES
	// PC SP Cloud Saves
	static void							InitSinglePlayerCloudSaves();
	static bool							RegisterSinglePlayerCloudSaveFile(const char* filename, const char* metadata);
	static bool							UnregisterSinglePlayerCloudSaveFile(const char* filename);
	static bool							UpdatedSinglePlayerCloudSaveEnabled(const char* rosTitleName);
#endif

#if RSG_ORBIS
	static void FlagIpReleasedOnGameResume(const bool bFlagIpReleasedOnGameResume);

	static bool HasActivePlayStationPlusEvent();            // we have an active PS+ event in our event schedule and events are enabled (via tunables)
	static bool IsPlayStationPlusCustomUpsellEnabled();     // PS+ custom upsell is enabled
	static bool IsPlayStationPlusPromotionEnabled();        // PS+ promotion (removing PS+ multiplayer requirement) is enabled
#endif

	static bool HasPendingProfileChange();
	static bool CanActionProfileChange(OUTPUT_ONLY(bool forceVerboseLogging = false));
	static void ActionProfileChange();
	static bool HasRecentResume();

	static void OnActionProfileChange();
	static void OnProfileChangeCommon();

#if RSG_NP
	// set from script to indicate that real time multiplayer is enabled / disabled
	// the requirements for this mean that we can't always detail the exact periods in code
	static void SetRealtimeMultiplayerScriptFlags(const unsigned scriptFlags);
	static bool HasRealtimeMultiplayerScriptFlags(const unsigned scriptFlags);
#endif 

#if NET_ENABLE_MEMBERSHIP_FUNCTIONALITY
	static bool HasShownMembershipWelcome() { return sm_HasShownMembershipWelcome; }
	static void SetHasShownMembershipWelcome();
	static bool IsSuppressingMembershipForScript() { return sm_SuppressMembershipForScript; }
	static void SetSuppressMembershipForScript(const bool suppress);
#endif

	static void OnTunablesRead();

private:

	//PURPOSE
	// Reset friends count in match.
	static void ResetFriendsInMatch();

	//PURPOSE
	// Handles the case where the gamer signs out or otherwise becomes
	// disconnected from the network while in multiplayer.
    static bool ShouldBailFromMultiplayer(); 
    static bool ShouldBailFromVoiceSession(); 
    static void BailOnMultiplayer(const eBailReason nBailReason, int nBailContext);
    static void BailOnVoiceSession();

	static void RefreshActivePlayer();
	static void SetMainGamerInfo(const rlGamerInfo& gamerInfo);
	static void ClearMainGamerInfo();
    static void RefreshPrivileges();

    static void CheckAndSendSubscriptionTelemetry();

	static void CheckAndSendMembershipTelemetry();
	static void UpdateMembership();

#if RSG_XBOX
	static void UpdatePrivilegeChecks();
	static void UpdatePermissionChecks();
#endif // RSG_XBOX

	static unsigned sm_LastResumeTimestamp;
	static int sm_ProfileChangeActionCounter;
	static bool sm_PostponeProfileChange;

	static unsigned sm_LastBrowserUpsellRequestTimestamp;

#if NET_ENABLE_MEMBERSHIP_FUNCTIONALITY
	static bool sm_HasShownMembershipWelcome;
	static bool sm_SuppressMembershipForScript;
	static bool sm_HasCachedMembershipReceivedEvent;
	static rlScMembershipStatusReceivedEvent sm_MembershipReceivedEvent;
	static bool sm_HasCachedMembershipChangedEvent;
	static rlScMembershipStatusChangedEvent sm_MembershipChangedEvent;
#endif

	// manage pending profile changes
	static void UpdatePendingProfileChange(const int timestep);
	static int GetProfileChangeActionDelayMs();

	static void UpdatePendingServiceActions();

	// manage real-time multiplayer tracking
#if RSG_NP
	static unsigned sm_RealtimeMultiplayerScriptFlags;
#if !__NO_OUTPUT
	static unsigned sm_PreviousRealtimeMultiplayerScriptFlags;
#endif
#endif

	//PURPOSE
	// Callback for presence events (signin, signout, etc.)
	static void OnPresenceEvent(const rlPresenceEvent* evt);

	//PURPOSE
	// Callback for Ros events 
	static void OnRosEvent(const rlRosEvent& evt);

#if ENABLE_SINGLE_PLAYER_CLOUD_SAVES
	// PURPOSE
	//	Callback for PC events
	static rlPcEventDelegator::Delegate sm_PcDlgt;
	static void OnPcEvent(rlPc* pc, const rlPcEvent* evt);
#endif

	//PURPOSE
	//	Callback for friend events
	static void OnFriendEvent(const rlFriendEvent* evt);
	static void RefreshFriendsRead();

#if NET_ENABLE_MEMBERSHIP_FUNCTIONALITY
	//PURPOSE
	//	Callback for membership events
	static void OnMembershipEvent(const rlScMembershipEvent* evt);
	static unsigned sm_EvaluateBenefitsInterval;
	static unsigned sm_LastEvaluateBenefitsTime;
	static bool sm_NeedsToEvaluateBenefits;
#endif

	// System Events
	static ServiceDelegate sm_ServiceDelegate;
	static void OnServiceEvent(sysServiceEvent* evt);

#if RSG_ORBIS
	static void UpdatePlayStationPlusEvent();
	static int GetActivePlayStationPlusEventId();           // PS+ event is enabled (via tunables or command line)
	static unsigned GetPlayStationPlusPromotionGraceMs();   // grace period to allow for gracefully exiting multiplayer when the promotion ends
#endif

#if RSG_NP
	static rage::rlNpEventDelegate sm_NpEventDelegate;
	static void OnNpEvent(rlNp* /*np*/, const rlNpEvent* evt);

	// code checks for real-time multiplayer
	static bool IsInRealTimeMultiplayer();
#endif

	static void UpdateMultiplayerTracking();

#if __BANK

private:
	static bool sm_ToggleCallback;

	static void LiveBank_Update();

	//Presence
	static void LiveBank_TestPresence();
	static void LiveBank_TestPresenceExhaustion();

	//Marketplace
	static void LiveBank_EnumerateOffers();
	static void LiveBank_EnumerateAssets();
	static void LiveBank_SpewOffers();
	static void LiveBank_SpewAssets();
	static void LiveBank_SpewCategories();
	static void LiveBank_GetDownloadStatus();
	static void LiveBank_CheckoutOffer();

	//Socket status
	static void LiveBank_SpewSocketStatus();

	//Privileges
	static void LiveBank_ShowSharingPrivileges();

	//Age
	static void LiveBank_ShowAgeGroup();

	//Game Region
	static void LiveBank_ShowGameRegion();

public:
	static void InitWidgets();
	static void WidgetShutdown();

	//Achievements
	static void LiveBank_RefreshAchInfo();
	static void LiveBank_AwardAch();
	static void LiveBank_AwardRockstarEditorAch();

#if __STEAM_BUILD
	static void LiveBank_GetSteamAchProgress();
	static void LiveBank_SetSteamAchProgress();
#endif // __STEAM_BUILD

#endif // __BANK

}; // End CLiveManager

#endif // CLIVEMANAGER_H

// eof

