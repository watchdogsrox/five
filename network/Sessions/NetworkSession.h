//
// NetworkSession.h
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef NETWORKSESSION_H
#define NETWORKSESSION_H

//Rage includes
#include "net/peercomplainer.h"
#include "snet/session.h"
#include "rline/rl.h"
#include "fwnet/netchannel.h"

//Game includes
#include "Network/NetworkTypes.h"
#include "Network/Live/NetworkMetrics.h"
#include "Network/Sessions/NetworkSessionUtils.h"
#include "Network/Sessions/NetworkSessionMessages.h"
#include "Network/Sessions/NetworkGameConfig.h"
#include "Network/Stats/NetworkLeaderboardSessionMgr.h"

#define RSG_PRESENCE_SESSION	(RSG_ORBIS)

namespace rage
{
	class sysMemAllocator;
	class netPlayer;
    class rlFriend;
	BANK_ONLY(class bkCombo;);
}

#if __BANK
class CNetGamePlayer;
#endif

//PURPOSE
// Stash for matchmaking attributes
class MatchmakingAttributes
{
public:

	enum eAttribute
	{
		MM_ATTR_AIM_TYPE,
		MM_ATTR_ACTIVITY_TYPE,
		MM_ATTR_ACTIVITY_ID,
		MM_ATTR_NUM,
	};

	struct sAttribute
	{
		sAttribute() : nValue(0), bHasBeenApplied(false) {}
		void Apply(unsigned v) { nValue = v; bHasBeenApplied = true; }
		void Clear() { bHasBeenApplied = false; }
		unsigned GetValue() { return nValue; }
		bool IsApplied() { return bHasBeenApplied; }

		unsigned nValue;
		bool bHasBeenApplied; 
	};

	sAttribute* GetAttribute(eAttribute a) { return &m_Attributes[a]; } 

	void Clear()
	{
		for(int i = 0; i < MM_ATTR_NUM; i++)
			m_Attributes[i].Clear();
	}

private:

	sAttribute m_Attributes[MM_ATTR_NUM]; 
};

struct sMatchmakingInfoMine
{
	sMatchmakingInfoMine()
		: m_nSessionEntryCount(0) 
		, m_SessionTimeTaken_s(0) 
		, m_SessionTimeTaken_s_Rolling(0)		
		, m_nPlayersOnEntry(0)
		, m_nPlayersOnEntry_Rolling(0)			
		, m_nSessionStartedPosix(0)
		, m_MatchmakingTimeTaken_s(0) 
		, m_nMatchmakingScMmCount(0)
		, m_nMatchmakingScMmPlayerAvg(0)
		, m_nMatchmakingTotalCount(0)
		, m_nMatchmakingTotalPlayerAvg(0)
		, m_nMatchmakingAttempts(0) 
		, m_nMatchmakingCount(0)
		, m_MatchmakingTimeTaken_s_Rolling(0) 	
		, m_nMatchmakingScMmCount_Rolling(0)		
		, m_nMatchmakingScMmPlayerAvg_Rolling(0)	
		, m_nMatchmakingTotalCount_Rolling(0)	
		, m_nMatchmakingTotalPlayerAvg_Rolling(0)
		, m_nMatchmakingAttempts_Rolling(0) 		
		, m_nSessionPlayersAdded(0)
		, m_nSessionPlayersRemoved(0)
		, m_LastPlayerAddedPosix(0) 
		, m_TimesQueried(0) 
		, m_LastQueryPosix(0)
		, m_nMigrationChurn(0)
		, m_nMigrationTimeTaken_s(0) 
		, m_nMigrationCandidateIdx(0) 
		, m_nMigrationCount(0)					
		, m_nMigrationChurn_Rolling(0)			
		, m_nMigrationTimeTaken_s_Rolling(0) 	
		, m_nMigrationCandidateIdx_Rolling(0) 	
		, m_nSoloReason(0xFF)
		, m_nNumTimesSolo(0)						
		, m_nNumTimesKicked(0)					
		, m_nStallCount(0) 
		, m_nStallCountOver1s(0)
	{

	}

	void ClearSession()
	{
		m_nPlayersOnEntry = 0;
		m_MatchmakingTimeTaken_s = 0;
		m_SessionTimeTaken_s = 0;
		m_nSessionStartedPosix = 0;
		m_nSessionPlayersAdded = 0;
		m_nSessionPlayersRemoved = 0;
		m_LastPlayerAddedPosix = 0; 
		m_TimesQueried = 0;
		m_LastQueryPosix = 0;
		m_nMigrationChurn = 0;
		m_nMigrationTimeTaken_s = 0;
		m_nMigrationCandidateIdx = 0; 
		ClearMatchmaking();
	}

	void ClearMatchmaking()
	{
		m_MatchmakingTimeTaken_s = 0;
		m_nMatchmakingScMmCount = 0;
		m_nMatchmakingScMmPlayerAvg = 0;
		m_nMatchmakingTotalCount = 0;
		m_nMatchmakingTotalPlayerAvg = 0;
		m_nMatchmakingAttempts = 0;
	}

	bool IncrementStat(u8& nStat)
	{
		if(nStat < MAX_UINT8)
		{
			nStat++;
			return true;
		}
		return false;
	}

	bool IncrementStat(u16& nStat)
	{
		if(nStat < MAX_UINT16)
		{
			nStat++;
			return true;
		}
		return false;
	}

	u8 m_nSessionEntryCount; 
	u8 m_SessionTimeTaken_s; 
	u8 m_SessionTimeTaken_s_Rolling;		// cumulative across all sessions
	u8 m_nPlayersOnEntry;
	u8 m_nPlayersOnEntry_Rolling;			// cumulative across all sessions
	u32 m_nSessionStartedPosix;
	u8 m_MatchmakingTimeTaken_s; 
	u8 m_nMatchmakingScMmCount;
	u8 m_nMatchmakingScMmPlayerAvg;
	u8 m_nMatchmakingTotalCount;
	u8 m_nMatchmakingTotalPlayerAvg;
	u8 m_nMatchmakingAttempts; 
	u8 m_nMatchmakingCount;					// cumulative across all sessions
	u8 m_MatchmakingTimeTaken_s_Rolling; 	// cumulative across all sessions
	u8 m_nMatchmakingScMmCount_Rolling;		// cumulative across all sessions
	u8 m_nMatchmakingScMmPlayerAvg_Rolling;	// cumulative across all sessions
	u8 m_nMatchmakingTotalCount_Rolling;	// cumulative across all sessions
	u8 m_nMatchmakingTotalPlayerAvg_Rolling;// cumulative across all sessions
	u8 m_nMatchmakingAttempts_Rolling; 		// cumulative across all sessions
	s16 m_nSessionPlayersAdded;
	s16 m_nSessionPlayersRemoved;
	u32 m_LastPlayerAddedPosix; 
	u16 m_TimesQueried; 
	u32 m_LastQueryPosix;
	s16 m_nMigrationChurn;
	u16 m_nMigrationTimeTaken_s; 
	u8 m_nMigrationCandidateIdx; 
	u16 m_nMigrationCount;					// cumulative across all sessions
	s16 m_nMigrationChurn_Rolling;			// cumulative across all sessions
	u16 m_nMigrationTimeTaken_s_Rolling; 	// cumulative across all sessions
	u8 m_nMigrationCandidateIdx_Rolling; 	// cumulative across all sessions
	u8 m_nSoloReason;						
	u16 m_nNumTimesSolo;					// cumulative across all sessions
	u16 m_nNumTimesKicked;					// cumulative across all sessions
	u16 m_nStallCount;						// cumulative across all sessions
	u16 m_nStallCountOver1s;				// cumulative across all sessions
};

//PURPOSE
// Application data for freemode sessions
struct SessionUserData_Freeroam
{
    SessionUserData_Freeroam() { Clear(); }

    void Clear();

    enum 
    {
        USERDATA_FLAG_CLOSED_FRIENDS = BIT0,
        USERDATA_FLAG_CLOSED_CREW = BIT1,
    };

	u16 m_ux, m_uy, m_uz;
	u8 m_nPropertyID[MAX_NUM_PHYSICAL_PLAYERS];
	u8 m_nPlayerAim;
	u16 m_nAverageCharacterRank; 
    u8 m_nTransitionPlayers; 
	u8 m_nAverageMentalState;
    u8 m_nBeacon;
    u8 m_nFlags;
    s32 m_nCrewID[MAX_UNIQUE_CREWS];
	u8 m_nNumBosses;
	WIN32PC_ONLY(u8 m_nAveragePedDensity;)
};

//PURPOSE
// Application data for activity sessions
struct SessionUserData_Activity
{
    SessionUserData_Activity() { Clear(); }

    void Clear();
    rlGamerHandle m_hContentCreator;
	u8 m_nPlayerAim;
    u16 m_nAverageCharacterRank; 
    u32 m_nMinimumRank; 
    u8 m_nBeacon;
    u16 m_nAverageELO;
	u8 m_bIsWaitingAsync;
	u8 m_bIsPreferred;
	u32 m_InProgressFinishTime; 
};

//PURPOSE
// Channel specific connection manager delegate
class ConnectionManagerDelegate
{
public:

	ConnectionManagerDelegate();
	void Init(netConnectionManager* pCxnMgr, unsigned nChannelID);
	void Shutdown(); 

	// handler for connection events on this delegate
	void OnConnectionEvent(netConnectionManager* pCxnMgr, const netEvent* pEvent);

private:

	netConnectionManager::Delegate m_CxnMgrDlgt;
	unsigned m_nChannelID;
};

//PURPOSE
// Game level session management support
class CNetworkSession
{
	friend class ConnectionManagerDelegate;
	struct sTransitionPlayer;

private:

	enum eSessionState
	{
		SS_INVALID = -1,
		SS_IDLE,
		SS_FINDING,
		SS_HOSTING,
		SS_JOINING,
		SS_ESTABLISHING,
		SS_ESTABLISHED,
		SS_BAIL,
		SS_HANDLE_KICK,
		SS_LEAVING,
		SS_DESTROYING,
		SS_MAX,
	};

	// transition
	enum eTransitionState
	{
		TS_INVALID = -1,
		TS_IDLE,
		TS_HOSTING,
		TS_JOINING,
		TS_ESTABLISHING,
		TS_ESTABLISHED,
		TS_LEAVING,
		TS_DESTROYING,
		TS_MAX,
	};

public:

	// timeout policy
	enum eTimeoutPolicy
	{
		POLICY_INVALID = -1,
		POLICY_NORMAL,
		POLICY_AGGRESSIVE,
		POLICY_MAX,
	};

public:

	struct sUniqueCrewData
	{
		sUniqueCrewData() { Clear(); }

		void Clear()
		{
			m_nLimit = 0;
			m_bOnlyCrews = true; 
			m_nMaxCrewMembers = 0;
			m_nCurrentUniqueCrews = 0;
			for(u8 i = 0; i < MAX_UNIQUE_CREWS; i++)
			{
				m_nActiveCrewIDs[i] = RL_INVALID_CLAN_ID;
				m_nCurrentNumMembers[i] = 0;
			}
		}

		u8 m_nLimit; 
		bool m_bOnlyCrews;
		u8 m_nMaxCrewMembers;
		u8 m_nCurrentUniqueCrews;
		rlClanId m_nActiveCrewIDs[MAX_UNIQUE_CREWS];
		u8 m_nCurrentNumMembers[MAX_UNIQUE_CREWS]; 
	};

public:

     CNetworkSession();
	~CNetworkSession();

	bool Init();
	void Shutdown(bool isSudden, eBailReason bailReasonIfSudden);
    void Update();

    // setup any session tunables
    void InitTunables();
    void OnCredentialsResult(bool bSuccess);
	void OnBail(const sBailParameters bailParams);
	void OnStallDetected(unsigned nStallLength);
	void OnReturnToSinglePlayer();
	void OnServiceEventSuspend();

	// session only (shutdown -> init)
	bool DropSession();

	// check before quickmatch or hosting - poll, will eventually return true
	bool CanEnterMultiplayer();

    // allows matchmaking to be customised
    void SetMatchmakingQueryState(MatchmakingQueryType nQuery, bool bEnabled);
    bool GetMatchmakingQueryState(MatchmakingQueryType nQuery);

#if RL_NP_SUPPORT_PLAY_TOGETHER
	// PlayTogether - will be sent a system invite when we a complete session join
	void RegisterPlayTogetherGroup(const rlGamerHandle* phGamers, unsigned nGamers);
	void InvitePlayTogetherGroup();
#endif

    // followers - will be sent a follow invite when we complete session joins
    void AddFollowers(rlGamerHandle* phGamers, unsigned nGamers);
    void RemoveFollowers(rlGamerHandle* phGamers, unsigned nGamers);
    bool HasFollower(const rlGamerHandle& hGamer);
    void ClearFollowers(); 
    void RetainFollowers(bool bRetain); 

	// finds sessions, attempts to join sessions and if either fails, hosts a session
	bool DoFreeroamMatchmaking(const int nGameMode, const int nSchemaID, const unsigned nMaxPlayers, unsigned matchmakingFlags);
	bool DoQuickMatch(const int nGameMode, const int nSchemaID, const unsigned nMaxPlayers, unsigned matchmakingFlags); 
	bool DoFriendMatchmaking(const int nGameMode, const int nSchemaID, const unsigned nMaxPlayers, unsigned matchmakingFlags);
	bool DoSocialMatchmaking(const char* szQueryName, const char* szParams, const int nGameMode, const int nSchemaID, const unsigned nMaxPlayers, unsigned matchmakingFlags);
	bool DoActivityQuickmatch(const int nGameMode, const unsigned nMaxPlayers, const int nActivityType, const unsigned nActivityID, unsigned matchmakingFlags);

	// session state flow
	bool HostSession(const int nGameMode, int nMaxPlayers, unsigned nHostFlags); 
    bool HostClosed(const int nGameMode, const int nMaxPlayers, unsigned nHostFlags);
    bool HostClosedFriends(const int nGameMode, const int nMaxPlayers, unsigned nHostFlags);
    bool HostClosedCrew(const int nGameMode, const int nMaxPlayers, const unsigned nUniqueCrewLimit, const unsigned nCrewLimitMaxMembers, unsigned nHostFlags);
    bool LeaveSession(const unsigned leaveFlags, const LeaveSessionReason leaveReason);
	
	// reserve session slots
	bool ReserveSlots(const SessionType sessionType, const rlGamerHandle* pHandles, const unsigned nHandles, const unsigned nReservationTime); 

	// join the session we have been invited to
	bool JoinInvite();
	bool ActionInvite();
    void WaitForInvite(JoinFailureAction nJoinFailureAction, const int nJoinFailureGameMode);
    void ClearWaitFlag();
    bool IsWaitingForInvite() const { return m_bIsInvitePendingJoin; }

	// join particular sessions - we have an option to hit matchmaking if these fail
	bool JoinPreviousSession(JoinFailureAction nJoinFailureAction, bool bRunDetailQuery = true);
	bool JoinSession(const rlSessionInfo& hInfo, bool bRunDetailQuery, JoinFailureAction nJoinFailureAction, const int nJoinFailureGameMode, rlSlotType nSlotType);
	bool JoinPreviouslyFailed();

    // running session detail query
    bool IsRunningDetailQuery() { return m_bRunningDetailQuery; }

    // access to session members
	unsigned GetSessionMemberCount() const;
	unsigned GetSessionMembers(rlGamerInfo aInfo[], const unsigned nMaxGamers) const;

	// join validation from script - after joining a session, we may still reject completion of this 
	// in script for additional reasons - allow script to validate the join so that we can re-enter MM
	void SetScriptValidateJoin();
	void ValidateJoin(bool bJoinSuccessful);

	// allow script to determine when the clock and weather should be updated
	void SetScriptHandlingClockAndWeather(bool bScriptHandling);

	// access to matchmaking groups
	void SetMatchmakingGroup(eMatchmakingGroup nGroup);
	eMatchmakingGroup GetMatchmakingGroup();
	void AddActiveMatchmakingGroup(eMatchmakingGroup nGroup);
	void ClearActiveMatchmakingGroups();
    void UpdateMatchmakingGroups();
	
	// access to matchmaking group maximums 
	void SetMatchmakingGroupMax(const eMatchmakingGroup nGroup, unsigned nMaximum);
	unsigned GetMatchmakingGroupMax(const eMatchmakingGroup nGroup) const;
    unsigned GetMatchmakingGroupNum(const eMatchmakingGroup nGroup) const;
    unsigned GetMatchmakingGroupFree(const eMatchmakingGroup nGroup) const;

	// unique crew tracking
	void SetUniqueCrewLimit(const SessionType sessionType, unsigned nUniqueCrewLimit);
    void SetUniqueCrewOnlyCrews(const SessionType sessionType, bool bOnlyCrews);
    void SetCrewLimitMaxMembers(const SessionType sessionType, unsigned nCrewLimitMaxMembers);
	unsigned GetUniqueCrewLimit(const SessionType sessionType) const;
	void UpdateCurrentUniqueCrews(const SessionType sessionType);
	
	// matchmaking parameters
	void SetMatchmakingPropertyID(u8 nPropertyID);
	void SetMatchmakingMentalState(u8 nMentalState);
	void SetMatchmakingMinimumRank(u32 nMinimumRank);
    void SetMatchmakingELO(u16 nELO);
	void SetEnterMatchmakingAsBoss(const bool bEnterMatchmakingAsBoss);

	// returns the length of time we have been in the session
	unsigned GetTimeInSession() const;

	// access to session type
	bool IsSoloMultiplayer() const;
	bool AreAllSessionsNonVisible() const;
	bool IsActivitySession() const;
    bool IsClosedSession(const SessionType sessionType) const;
    bool IsClosedFriendsSession(const SessionType sessionType) const;
    bool IsClosedCrewSession(const SessionType sessionType) const;
	bool HasCrewRestrictions(const SessionType sessionType) const;
	bool IsSoloSession(const SessionType sessionType) const;
	bool IsPrivateSession(const SessionType sessionType) const;
	eSessionVisibility GetSessionVisibility(const SessionType sessionType) const;
    bool MarkAsGameSession();

	// invite management
	bool SendPlatformInvite(const rlSessionToken& sessionToken, const rlGamerHandle* pGamers, const unsigned nGamers);

    // delayed presence invites
    bool AddDelayedPresenceInvite(
		const rlGamerHandle& hGamer,
		const bool bIsImportant,
		const bool isTransition,
		const char* szContentId,
		const char* pUniqueMatchId,
		const int nInviteFrom,
		const int nInviteType,
		const unsigned nPlaylistLength,
		const unsigned nPlaylistCurrent);
    void SendDelayedPresenceInvites();

	// send an invite
    bool SendInvites(const rlGamerHandle* pGamers, const unsigned nGamers, const char* pSubject = nullptr, const char* pMessage = nullptr, netStatus* pStatus = nullptr);
	bool SendPresenceInvites(
		const rlGamerHandle* pGamers, 
		const bool* pbImportant, 
		const unsigned nGamers, 
		const char* szContentId, 
		const char* pUniqueMatchId,
		const int nInviteFrom,
		const int nInviteType,
		const unsigned nPlaylistLength,
		const unsigned nPlaylistCurrent);

	void RemoveInvites(const rlGamerHandle* pGamers, const unsigned nGamers);
	void RemoveAllInvites();
	bool CancelInvites(const rlGamerHandle* pGamers, const unsigned nGamers);
    void RemoveAndCancelAllInvites();

	// access to in-session invite information
	bool DidJoinViaInvite()				{ return m_bJoinedViaInvite; }
	const rlGamerHandle& GetInviter()	{ return m_Inviter; }

	// access to structures
    CNetworkRecentPlayers& GetRecentPlayers()					{ return m_RecentPlayers; }
    CBlacklistedGamers& GetBlacklistedGamers()			{ return m_BlacklistedGamers; }
    snSession& GetSnSession()							{ return *m_pSession; }
	MatchmakingAttributes& GetMatchmakingAttributes()	{ return m_MatchmakingAttributes; }
    const NetworkGameConfig& GetMatchConfig() const;

	// config changing
	void SetSessionInProgress(const SessionType sessionType, const bool bIsInProgress);
	bool GetSessionInProgress(const SessionType sessionType);
	void SetContentCreator(const SessionType sessionType, const unsigned nContentCreator);
	unsigned GetContentCreator(const SessionType sessionType);
	void SetActivityIsland(const SessionType sessionType, const int nActivityIsland);
	int GetActivityIsland(const SessionType sessionType);
	void SetSessionVisibility(const SessionType sessionType, const bool bIsVisible);
    bool IsSessionVisible(const SessionType sessionType) const;
    void SetSessionVisibilityLock(const SessionType sessionType, bool bLockVisibility, bool bLockSetting);
    bool IsSessionVisibilityLocked(const SessionType sessionType) const;
    void ChangeSessionSlots(const unsigned nPublic, const unsigned nPrivate);
	
	//PURPOSE
	// Blocks incoming join requests (required to be session host)
	void SetBlockJoinRequests(bool bBlockJoinRequests);
	bool IsBlockingJoinRequests() { return IsHost() && m_bBlockJoinRequests; }

	//PURPOSE
	// Returns true if we kicked a player from the session
	bool KickPlayer(const netPlayer* pPlayerToKick, const KickReason kickReason, const unsigned nComplaints, const rlGamerHandle& hGamer);
	bool KickRandomPlayer(const bool isSpectator, const rlGamerHandle& handleToExclude, const KickReason kickReason);

	//PURPOSE
	// Returns the player hosting the session
	netPlayer* GetHostPlayer() const;

	//PURPOSE
	// Returns true when the single player scripts have been cleaned up or timed out
	bool SendEventStartAndWaitSPScriptsCleanUp();

	// state queries
	int  GetSessionStateAsInt() const		{ return static_cast<int>(m_SessionState); }
	bool IsSessionIdle() const				{ return m_SessionState == SS_IDLE; }
	bool IsSessionActive() const			{ return !IsSessionIdle(); }
	bool IsSessionCreated() const			{ return m_SessionState >= SS_ESTABLISHING; }
	bool IsSessionEstablished() const		{ return m_SessionState == SS_ESTABLISHED; }
	bool IsSessionLeaving() const			{ return m_SessionState >= SS_LEAVING; } 
	bool CanChangeSessionAttributes() const	{ return m_SessionState <= SS_ESTABLISHED; }
	bool IsBusy() const			
	{ 
		if(m_pSessionStatus->Pending())
			return true;
		for(int i = 0; i < MatchmakingQueryType::MQT_Num; i++) if(m_MatchMakingQueries[i].IsBusy())
			return true; 
		return false;
	}

	bool IsMigrating() const	{ return m_bIsMigrating; }

	// checks if session is in suitable state before performing operations
	bool CanStart() const;
	bool CanLeave() const		{ return (m_SessionState > SS_IDLE) && (m_SessionState <= SS_ESTABLISHED) && !IsBusy(); } 

	bool IsHostPlayerAndRockstarDev(const rlGamerHandle& handle ) const;

	// session queries
    bool IsInvitable() const;
    bool IsOnline() const;
    bool IsLan() const;
    bool IsHost() const;
	bool IsSessionMember(const rlGamerHandle& hGamer) const;
    bool GetSessionMemberInfo(const rlGamerHandle& hGamer, rlGamerInfo* pInfo) const;
    bool GetSessionNetEndpointId(const rlGamerInfo& sInfo, EndpointId* endpointId);

	unsigned GetMaximumNumPlayers() { return m_pSession->GetMaxSlots(RL_SLOT_PUBLIC) + m_pSession->GetMaxSlots(RL_SLOT_PRIVATE); }
	unsigned GetCurrentNumPlayers() { return m_pSession->GetGamerCount(); }
	unsigned GetNumAvailableSlots() { return m_pSession->GetAvailableSlots(RL_SLOT_PUBLIC) + m_pSession->GetAvailableSlots(RL_SLOT_PRIVATE); }
	bool IsSessionFull() { return (GetCurrentNumPlayers() >= GetMaximumNumPlayers()); }

	// checks if either the session or transition session match this info
	bool IsInSession(const rlSessionInfo& hInfo);
	bool IsJoiningSession(const rlSessionInfo& hInfo);
	bool IsMySessionToken(const rlSessionToken& hToken);
    bool IsMyTransitionToken(const rlSessionToken& hToken);
	NetworkGameConfig* GetInviteDetails(rlSessionInfo* pInfo); 

	// query invite trackers
	bool AckInvite(const rlGamerHandle& hGamer);
	bool SetInviteDecisionMade(const rlGamerHandle& hGamer);
	bool SetInviteStatus(const rlGamerHandle& hGamer, unsigned nCurrentStatus);
	unsigned GetInviteStatus(const rlGamerHandle& hGamer);
	bool IsInviteDecisionMade(const rlGamerHandle& hGamer);

	const CNetworkInvitedPlayers& GetInvitedPlayers() const { return m_InvitedPlayers; }
	const CNetworkInvitedPlayers& GetTransitionInvitedPlayers() const { return m_TransitionInvitedPlayers; }

    s32 GetHostAimPreference() { return m_nHostAimPreference; }

	// join friend function - like 360 JIP
	static bool JoinFriendSession(const rlFriend* pFriend);

    // activity spectators
	void SetActivityPlayersMax(unsigned nPlayersMax);
	void SetActivitySpectatorsMax(unsigned nSpectatorsMax);
    unsigned GetActivityPlayerMax(bool bSpectator);
    unsigned GetActivityPlayerNum(bool bSpectator);
    void SetActivitySpectator(bool bIsSpectator);
    bool IsActivitySpectator() { return m_TransitionData.m_PlayerData.m_bIsSpectator; }
    bool IsActivitySpectator(const rlGamerHandle& hGamer);
	
	// transition functions
	bool HostTransition(const int nGameMode, unsigned nMaxPlayers, const int nActivityType, const unsigned nActivityID, const int nActivityIsland, const unsigned nContentCreator, unsigned nHostFlags);
	bool JoinTransition(const rlSessionInfo& tSessionInfo, const rlSlotType kSlotType, unsigned nJoinFlags, const rlGamerHandle* pHandles = nullptr, const unsigned nGamers = 0);
	bool JoinPreviouslyFailedTransition();
	bool LeaveTransition();
	void CancelTransitionMatchmaking();
    void BailTransition(eTransitionBailReason nBailReason, const int nContextParam1 = -1, int const nContextParam2 = -1, const int nContextParam3 = -1);
	
    bool JoinPlayerTransition(CNetGamePlayer* pPlayer);
	bool LaunchTransition();
	void SetDoNotLaunchFromJoinAsMigratedHost(bool bLaunch);

	void SetTransitionActivityType(int nActivityType);
	void SetTransitionActivityID(unsigned nActivityID);
	void ChangeTransitionSlots(const unsigned nPublic, const unsigned nPrivate);

	bool SendTransitionInvites(const rlGamerHandle* pGamers, const unsigned nGamers, const char* pSubject = nullptr, const char* pMessage = nullptr);
	bool SendPresenceInvitesToTransition(
		const rlGamerHandle* pGamers,
		const bool* pbImportant,
		const unsigned nGamers,
		const char* szContentId,
		const char* pUniqueMatchId,
		const int nInviteFrom,
		const int nInviteType,
		const unsigned nPlaylistLength,
		const unsigned nPlaylistCurrent);
	void AddTransitionInvites(const rlGamerHandle* pGamers, const unsigned nGamers);
	void RemoveTransitionInvites(const rlGamerHandle* pGamers, const unsigned nGamers);
	void RemoveAllTransitionInvites();
	bool CancelTransitionInvites(const rlGamerHandle* pGamers, const unsigned nGamers);
    void RemoveAndCancelAllTransitionInvites();

	unsigned GetNumTransitionNonAsyncGamers();

    unsigned GetTransitionMemberCount();
	unsigned GetTransitionMembers(rlGamerInfo aInfo[], const unsigned nMaxGamers) const;
	unsigned GetTransitionMaximumNumPlayers() { return m_pTransition->GetMaxSlots(RL_SLOT_PUBLIC) + m_pTransition->GetMaxSlots(RL_SLOT_PRIVATE); }
	bool IsTransitionMember(const rlGamerHandle& hGamer) const;
	bool IsTransitionMemberSpectating(const rlGamerHandle& hGamer) const;
	bool IsTransitionMemberAsync(const rlGamerHandle& hGamer) const;
	bool GetTransitionNetEndpointId(const rlGamerInfo& sInfo, EndpointId* endpointId);
	bool GetTransitionMemberInfo(const rlGamerHandle& hGamer, rlGamerInfo* pInfo) const;

	int  GetTransitionStateAsInt() const { return static_cast<int>(m_TransitionState); }
	bool IsTransitionIdle() { return m_TransitionState == TS_IDLE; }
	bool IsTransitionActive() { return !IsTransitionIdle(); }
	bool IsTransitionHostPending() { return m_TransitionState == TS_HOSTING; }
	bool IsTransitionCreated() const { return m_TransitionState >= TS_ESTABLISHING; }
	bool IsTransitionEstablished() { return m_TransitionState == TS_ESTABLISHED; }
	bool IsTransitionLeaving() { return m_TransitionState >= TS_LEAVING; }
	bool IsLaunchingTransition() { return m_bLaunchingTransition; }
	bool IsActivityStarting() { return m_bTransitionStartPending; }
	bool IsTransitionHost() { return m_pTransition->IsHost(); }
	bool IsTransitionStatusPending() { return m_pTransitionStatus->Pending(); }
    bool IsTransitionMatchmaking() { return m_bIsTransitionMatchmaking; }
	bool IsTransitionLeavePostponed() { return m_bIsTransitionToGamePendingLeave; }
	bool IsInCorona() { return IsTransitionEstablished() && !IsTransitionLeaving() && !IsActivitySession(); }

    bool GetTransitionHost(rlGamerHandle* pHandle); 
	void OnReceivedTransitionSessionInfo(const CNetGamePlayer* pPlayer, bool bInvalidate);

	sTransitionPlayer* AddTransitionPlayer(const rlGamerInfo& hInfo, const EndpointId endpointId, const void* pData, const unsigned nSizeofData);
	void ApplyTransitionParameter(u16 nID, u32 nValue, bool bForceDirty);
	void ApplyTransitionParameter(int nIndex, const char* szValue, bool bForceDirty);
	void ClearTransitionParameters();
    void ForceSendTransitionParameters(const rlGamerInfo& hInfo);
	bool SendTransitionGamerInstruction(const rlGamerHandle& hGamer, const char* szGamerName, const int nInstruction, const int nInstructionParam, const bool bBroadcast);
    bool MarkTransitionPlayerAsFullyJoined(const rlGamerHandle& hGamer);
    bool HasTransitionPlayerFullyJoined(const rlGamerHandle& hGamer);
    
	bool DoTransitionQuickmatch(const int nGameMode, unsigned nMaxPlayers, const int nActivityType, const unsigned nActivityID, const int nActivityIsland, const unsigned nMmFlags, rlGamerHandle* pGamers = nullptr, unsigned nGamers = 0);
    bool JoinGroupActivity();
	void ClearGroupActivity();
	void RetainActivityGroupOnQuickmatchFail();
	bool IsNextJobTransition(const rlGamerHandle& hGamer, const rlSessionInfo& hInfo);
    
	void MarkAsPreferredActivity(bool bIsPreferred);
	void MarkAsWaitingAsync(bool bIsWaitingAsync);
	void SetInProgressFinishTime(unsigned nInProgressFinishTime);
	void SetActivityCreatorHandle(const rlGamerHandle& hGamer);
	void SetNumBosses(const unsigned nNumBosses);

    bool CanTransitionToGame(); 
	bool DoTransitionToGame(const int nGameMode, rlGamerHandle* pGamers, unsigned nGamers, bool bSocialMatchmaking, unsigned nMaxPlayers, unsigned nMmFlags);
    bool DoTransitionToNewGame(const int nGameMode, rlGamerHandle* pGamers, unsigned nGamers, unsigned nMaxPlayers, unsigned nHostFlags, const bool bAllowPreviousJoin);
    bool IsInTransitionToGame() { return m_bIsTransitionToGame || m_hTransitionHostGamer.IsValid(); }

    void OnSessionEntered(const bool bAsHost);
    void UpdateSessionTypes();
	SessionTypeForTelemetry GetSessionTypeForTelemetry() const { return m_SessionTypeForTelemetry; }

	void SetBlockTransitionJoinRequests(bool m_bBlockTransitionJoinRequests);
	bool IsBlockingTransitionJoinRequests() { return IsTransitionHost() && m_bBlockTransitionJoinRequests; }

	// access to in-session invite information
	bool DidJoinTransitionViaInvite() { return m_bJoinedTransitionViaInvite; }
	const rlGamerHandle& GetTransitionInviter()	{ return m_TransitionInviter; }

	// checks for whether we are entering or exiting multiplayer
	bool IsTransitioning();

    bool IsSessionHost(const rlSessionInfo& hInfo);
    
    // presence session
#if RSG_PRESENCE_SESSION
	snSession* GetCurrentMainSession(); 
	snSession* GetCurrentPresenceSession();
#endif
	void SetPresenceInvitesBlocked(const SessionType sessionType, bool bBlocked);
	bool IsJoinViaPresenceBlocked(const SessionType sessionType);

#if RSG_DURANGO
	static bool RegisterPlatformPartySession(const snSession* pSession);
	static bool DisassociatePlatformPartySession(const snSession* pSession);
	static bool CanDisassociatePlatformPartySession() { return !m_SessionDisassociateStatus.Pending(); }
	static bool RemoveFromParty(bool bNotInSoloParty);

	// need this pointer
	bool IsInPlatformPartyGameSession();
#endif

	void OnPartyChanged(); 
	void OnPartySessionInvalid();

    // store
    bool DidGamerLeaveForStore(const rlGamerHandle& hGamer);

	void SendStallMessage(const unsigned stallIncludingNetworkUpdate, const unsigned networkUpdateTime);

    // message sending
    template<typename T>
    void SendSessionMessage(const rlGamerHandle& hGamer, const T& tMsg)
    {
        // grab gamer info
        rlGamerInfo hInfo;
        if(m_pSession->GetGamerInfo(hGamer, &hInfo))
            m_pSession->SendMsg(hInfo.GetPeerInfo().GetPeerId(), tMsg, NET_SEND_RELIABLE);
    }

    // message sending
    template<typename T>
    void SendTransitionMessage(const rlGamerHandle& hGamer, const T& tMsg)
    {
        // grab gamer info
        rlGamerInfo hInfo;
        if(m_pTransition->GetGamerInfo(hGamer, &hInfo))
            m_pTransition->SendMsg(hInfo.GetPeerInfo().GetPeerId(), tMsg, NET_SEND_RELIABLE);
    }

    // setup policies based on timeout values
    static void InitRegistrationPolicies();

	// accessor to main Session unique crew tracking.
	const sUniqueCrewData& GetMainSessionCrewData() const { return m_UniqueCrewData[SessionType::ST_Physical]; }

	// queued join request
	bool AddQueuedJoinRequest(const rlGamerHandle& hGamer, bool bIsSpectator, s64 nCrewID, bool bConsumePrivate, unsigned nUniqueToken);
	bool CanQueueForPreviousSessionJoin();
	unsigned GetJoinQueueToken();
	const rlGamerHandle& GetJoinQueueGamer();
	void ClearQueuedJoinRequest();
	void RemoveAllQueuedJoinRequests();
	bool SendQueuedJoinRequest();
	void HandleJoinQueueInviteReply(const rlGamerHandle& hFrom, const rlSessionToken& sessionToken, const bool bDecisionMade, const bool bDecision, const unsigned nStatusFlags);
	void HandleJoinQueueUpdate(const bool bCanQueue, const unsigned nUniqueToken);
	
	// player card
	bool SendPlayerCardData(const rlGamerHandle& handle, const u32 sizeOfData, u8* data);
	bool SendPlayerCardDataRequest(const rlGamerHandle& handle);

	void SetSessionStateToBail(eBailReason reason)
	{
		gnetDebug1("SetSessionStateToBail :: Reason: %u", reason);
		m_nBailReason = reason; 
		SetSessionState(SS_BAIL, true);
		return;
	};

private:

	struct sMatchmakingQuery;
	struct sMatchmakingResults;

#if !__NO_OUTPUT
	const char* GetSessionStateAsString(eSessionState nState = SS_INVALID);
	const char* GetTransitionStateAsString(eTransitionState nState = TS_INVALID);
#endif

    //PURPOSE
	//  Receive player card stats from transition members.
	void ReceivePlayerCardData(const rlGamerId& gamerID, const u32 sizeOfData, u8* data);

	//PURPOSE
	//  Request local player player card stats.
	void ReceivePlayerCardDataRequest(const rlGamerId& gamerID);

	//PURPOSE
	// Maximum number of invite to cancel in the same request to CGameInviteCancel::Trigger
	static const unsigned int MAX_INVITES_TO_CANCEL = 100;

	enum
	{
		MAX_WAIT_FRAMES_SINGLE_PLAYER_SCRIPT_END_BEFORE_START = 5,
	};

	const rlSession* OnSessionQueryReceived(const rlSession* pSession, const rlGamerHandle& hGamer, rlSessionInfo* pInfo);
	void OnGetInfoMine(const rlSession* session, rlSessionInfoMine* pInfoMine);

#if RSG_PRESENCE_SESSION
	// presence session
	bool InitPresenceSession();
    void DropPresenceSession();
	bool HostPresenceSession();
	bool LeavePresenceSession(); 
	void UpdatePresenceSession();
	
	void OnSessionEventPresence(snSession* pSession, const snEvent* pEvent);
	void OnGetGamerDataPresence(snSession* pSession, const rlGamerInfo& gamerInfo, snGetGamerData* pData) const;
	bool OnHandleJoinRequestPresence(snSession* pSession, const rlGamerInfo& gamerInfo, snJoinRequest* pRequest);

	// transition
	enum ePresenceState
	{
		PS_IDLE,
		PS_HOSTING,
		PS_ESTABLISHED,
		PS_LEAVING,
		PS_DESTROYING,
	};

	ePresenceState m_nPresenceState;
	snSession m_PresenceSession;
	netStatus m_PresenceStatus;
	netStatus m_ModifyPresenceStatus;
    bool m_bPresenceSessionInvitesBlocked;

	snSession::Delegate m_PresenceSessionDlgt;

	bool m_bHasPendingForwardSession;
	snSession* m_pPendingForwardSession;
    snSession* m_pForwardSession;

	bool m_bRequirePresenceSession;
	bool m_bPresenceSessionFailed;
#endif

#if RSG_NP || __STEAM_BUILD
	void UpdatePresenceBlob();
	rlSessionInfo m_SessionInfoBlob;
#elif RSG_DURANGO
	static netStatus m_SessionRegisterStatus;
	static netStatus m_SessionDisassociateStatus;
#endif

#if RSG_PC
public:
	// Access the last checked time, so we can update it accordingly
	sysObfuscated<unsigned int> GetDinputCheckTime() { return m_dinputCheckTime;}

	// Access the last checked time, so we can update it accordingly
	sysObfuscated<unsigned int> GetDinputCount() { return m_dinputCount;}

private:
	// Obfuscated variable to count the number of instances of dinput8 for our simple mod detection
	sysObfuscated<unsigned int> m_dinputCount;

	// Obfuscated latest time-stamp, to know when to fire
	sysObfuscated<unsigned int> m_dinputCheckTime;

	// Set the number of dinput8's
	inline void SetDinputCount(unsigned int _count)
	{
		m_dinputCount.Set(_count);
	}
#endif

    bool IsNetworkMemoryFree(); // of nefarious single player thievery

    // common function to check we have the necessary setup to access multiplayer
    bool VerifyMultiplayerAccess(const char* szFunction, bool bCountLAN = true);

	// set current state of session
	void SetSessionState(const eSessionState nState, bool bSkipChangeLogic = false);

	// returns number / maximum of players in a matchmaking group
    int GetActiveMatchmakingGroupIndex(eMatchmakingGroup nGroup) const;

	// can another player join into the indicated matchmaking group
	bool CanJoinMatchmakingGroup(const eMatchmakingGroup nGroup) const;

    // returns whether this session can accept any joiners
    bool CanAcceptJoiners();

	// matchmaking functions
	bool DoMatchmakingCommon(const int nGameMode, const int nSchemaID, unsigned nMaxPlayers, unsigned nMmFlags); 
	void ApplyMatchmakingRegionAndLanguage(NetworkGameFilter* pFilter, const bool bUseRegion, const bool bUseLanguage);
	bool IsSessionCompatible(NetworkGameFilter* pFilter, rlSessionDetail* pDetails, const unsigned nMmFlags OUTPUT_ONLY(, bool bLog));
	unsigned ApplySessionFilter(NetworkGameFilter* pFilter, rlSessionDetail* pDetails, int nResults, const unsigned nMmFlags);

	bool ValidateMatchmakingQuery(sMatchmakingQuery* pQuery);
	bool FindSessions(sMatchmakingQuery* pQuery);
	bool FindSessionsSocial(sMatchmakingQuery* pQuery, const char* szQueryName, const char* szQueryParams);
	bool FindSessionsFriends(sMatchmakingQuery* pQuery, const bool bFriendMatchmaking);
	bool FindActivitySessions(sMatchmakingQuery* pQuery); 

	// host functions
	bool HostSession(NetworkGameConfig& tConfig, const unsigned nHostFlags);

	// join functions
	bool BuildJoinRequest(u8* pData, unsigned nMaxSize, unsigned& nSizeOfData, unsigned joinFlags);
	void BuildTransitionPlayerData(CNetGamePlayerDataMsg* pMsg, const sTransitionPlayer* const pGamer) const;
	bool JoinMatchmakingSession(const unsigned mmIndex, const rlGamerHandle* pHandles = nullptr, const unsigned nGamers = 0);
	bool JoinSession(const rlSessionInfo& tSessionInfo, const rlSlotType kSlotType, unsigned nJoinFlags, const rlGamerHandle* pReservedGamers = nullptr, const unsigned numReservedGamers = 0);
    bool DoJoinFailureAction();

	// leave / destroy functions
	void ClearFriendSessionStatus();
	void CaptureSessionData(); 
	void Destroy();
	void ClearSession(const unsigned leaveFlags, const LeaveSessionReason leaveReason);

    // manage state
	void UpdateTransitionToGame(); 
    void UpdateUserData();

	// utility to get a player
	CNetGamePlayer* GetPlayerFromConnectionID(int nCxnID, unsigned nChannelID);
	CNetGamePlayer* GetPlayerFromEndpointId(const EndpointId endpointId, unsigned nChannelID);

	// network events from rage
    void OnSessionEvent(snSession* pSession, const snEvent* pEvent);
	void OnConnectionEvent(netConnectionManager* pCxnMgr, const netEvent* pEvent);

	// peer complainer 
	void InitPeerComplainer(netPeerComplainer* pPeerComplainer, snSession* pSession);
	void MigrateOrInitPeerComplainer(netPeerComplainer* pPeerComplainer, snSession* pSession, const EndpointId serverEndpointId);
	void TryPeerComplaint(u64 nPeerID, netPeerComplainer* pPeerComplainer, snSession* pSession, const SessionType sessionType OUTPUT_ONLY(, bool bLogRejections = false));
	bool IsReadyForComplaints(const SessionType sessionType);
	void GetPeerComplainerPolicies(netPeerComplainer::Policies* pPolicies, snSession* pSession, eTimeoutPolicy nPolicy);
	void SetPeerComplainerPolicies(netPeerComplainer* pPeerComplainer, snSession* pSession, eTimeoutPolicy nPolicy);

	netNatType GetNatTypeFromPeerID(const SessionType sessionType, const u64 nPeerID);
	void OnPeerComplainerBootRequest(netPeerComplainer* pPeerComplainer, netPeerComplainer::BootCandidate* pCandidates, const unsigned nCandidates);

	// session callbacks
	bool OnHandleJoinRequest(snSession* pSession, const rlGamerInfo& gamerInfo, snJoinRequest* pRequest, SessionType sessionType);
	bool OnHandleGameJoinRequest(snSession* pSession, const rlGamerInfo& gamerInfo, snJoinRequest* pRequest);
	bool OnHandleTransitionJoinRequest(snSession* pSession, const rlGamerInfo& gamerInfo, snJoinRequest* pRequest);
	void OnGetGameGamerData(snSession* pSession, const rlGamerInfo& gamerInfo, snGetGamerData* pData) const;
	
	// migration callbacks
	bool ShouldMigrate();
	bool ShouldMigrateTransition();
	unsigned GetMigrationCandidates(snSession* pSession, rlPeerInfo* pCandidates, const unsigned nMaxCandidates);
    unsigned GetMigrationCandidatesTransition(snSession* pSession, rlPeerInfo* pCandidates, const unsigned nMaxCandidates);

	// utility for migration messages
	void SendMigrateMessages(const u64 peerID, snSession* pSession, const bool bStarted);

    // matchmaking
	void JoinOrHostSession(const unsigned sessionIndex);
	void CheckForAnotherSessionOrBail(eBailReason nBailReason, int nBailContext);

    void SetSessionPolicies(eTimeoutPolicy nPolicy);
    void SetSessionTimeout(unsigned nTimeout);
	unsigned GetSessionTimeout();

    void OpenNetwork();
    void CloseNetwork(bool bFullClose); 

#if !__NO_OUTPUT
	void DumpEstablishingStateInformation();
#endif

	// session flow
    void ProcessFindingState();
    void ProcessHostingState();
	void ProcessJoiningState();
	void ProcessEstablishingState();
	void ProcessHandleKickState();
		
	// session flow utility
	void CompleteSessionCreate();
	
	// process snEventXXX functions
	void ProcessAddingGamerEvent(snEventAddingGamer* pEvent);
    void ProcessAddedGamerEvent(snEventAddedGamer* pEvent);
	void ProcessRemovedGamerEvent(snEventRemovedGamer* pEvent);
	void ProcessSessionMigrateStartEvent(snEventSessionMigrateStart* pEvent);
	void ProcessSessionMigrateEndEvent(snEventSessionMigrateEnd* pEvent);
	void ProcessSessionJoinedEvent(snEventSessionJoined* pEvent);
    void ProcessSessionJoinFailedEvent(snEventJoinFailed* pEvent);

#if RSG_DURANGO
	void ProcessDisplayNameEvent(snEventSessionDisplayNameRetrieved* pEvent);
#endif

	// unique crew
    void ProcessCrewID(rlClanId nClanID, unsigned& nNoCrewPlayers, const SessionType sessionType);
	bool CanJoinWithCrewID(const SessionType sessionType, rlClanId nClanID);

	// kick functions
	void HandleLocalPlayerBeingKicked(const KickReason kickReason);
	void SendKickTelemetry(
		const snSession* session,
		const KickSource kickSource,
		const unsigned numSessionMembers,
		const unsigned sessionIndex,
		const unsigned timeSinceEstablished,
		const unsigned timeSinceLastMigration,
		const unsigned numComplainers,
		const rlGamerHandle& hComplainer);

	// session blacklisting
	void ClearBlacklistedSessions();
	bool IsSessionBlacklisted(const rlSessionToken& nSessionToken);
	bool AddJoinFailureSession(const rlSessionToken& nSessionToken);
	bool HasSessionJoinFailed(const rlSessionToken& nSessionToken);

public:

	// updates the values sent to the servers for SC presence info
	void UpdateSCPresenceInfo();

	inline void ForcePresenceUpdate() 
    { 
        m_bForceScriptPresenceUpdate = true; 
        UpdateSCPresenceInfo(); 
    }

	inline bool GetAndResetForceScriptPresenceUpdate() 
    { 
		const bool val = m_bForceScriptPresenceUpdate; 
		m_bForceScriptPresenceUpdate = false; 
		return val; 
	}
#if RSG_PC
	void CheckDinputCount();
#endif

private:

	enum eMatchmakingCriteria
    {
        CRITERIA_COMPOSITION,
        CRITERIA_PLAYER_COUNT,
        CRITERIA_FREEROAM_PLAYERS,
        CRITERIA_LOCATION,
        CRITERIA_RANK,
		CRITERIA_PROPERTY,
		CRITERIA_MENTAL_STATE,
		CRITERIA_MINIMUM_RANK,
        CRITERIA_BEACON,
        CRITERIA_ELO,
		CRITERIA_ASYNC,
		CRITERIA_REGION,
		CRITERIA_BOSS_DEPRIORITISE,
		CRITERIA_BOSS_REJECT,
		CRITERIA_ISLAND,
		CRITERIA_IN_PROGRESS_TIME_REMAINING,
#if __WIN32PC
		CRITERIA_PED_DENSITY,
#endif
		CRITERIA_NUM,
    };

    struct sMatchmakingCandidate
    {
		sMatchmakingCandidate() 
			: m_fScore(0.0f)
			, m_nCandidateID(0)
		{
			m_pDetail = nullptr;
			for(unsigned i = 0; i < MatchmakingQueryType::MQT_Num; i++)
				m_nHits[i] = 0;
		}

		static bool Sort(const sMatchmakingCandidate& a, const sMatchmakingCandidate& b) 
		{ 
			if(a.m_fScore < b.m_fScore)
				return true;
			// if equal, sort by player count
			else if(a.m_fScore == b.m_fScore)
				return (a.m_pDetail->m_NumFilledPrivateSlots + a.m_pDetail->m_NumFilledPublicSlots) > (b.m_pDetail->m_NumFilledPrivateSlots + b.m_pDetail->m_NumFilledPublicSlots);

			return false;
		}

        rlSessionDetail* m_pDetail;
        u8 m_nHits[MatchmakingQueryType::MQT_Num];
        float m_fScore;
        u8 m_nCandidateID;
    };

    bool IsMatchmakingPending(sMatchmakingQuery* pQuery);
	void ProcessMatchmakingResults(sMatchmakingQuery* pQueries, unsigned nQueries, sMatchmakingResults* pResults, const unsigned nMmFlags, const u64 nUniqueID);
	void AddMatchmakingResults(sMatchmakingCandidate* pCandidates, unsigned nCandidates, sMatchmakingResults* pResults, bool bBlacklisted);
	void DoSortMatchmakingPreference(NetworkGameFilter* pFilter, sMatchmakingCandidate* pCandidates, unsigned nCandidates, const unsigned nMmFlags);

	// transition functions
	bool InitTransition();
    void DropTransition();
	void ClearTransition();
    void UpdateTransition();
    void UpdateActivitySlots();

    void FailTransitionHost();
    void FailTransitionJoin();

	void UpdateTransitionMatchmaking();
    void UpdateTransitionGamerData();

    void SetTransitionState(eTransitionState nState, bool bSkipChangeLogic = false);

	bool HostTransition(const NetworkGameConfig& tConfig, const unsigned nHostFlags);

	void JoinOrHostTransition(unsigned sessionIndex);
    void CompleteTransitionQuickmatch();

	void SendLaunchMessage(u64 nPeerID = 0); 
	bool DoLaunchTransition();
	void SwapToGameSession();
	void SwapToTransitionSession();
    void SwapSessions(); 

	snSession* GetCurrentSession(bool bAllowLeaving = false);
	snSession* GetSessionFromChannelID(unsigned nChannelID, bool bAllowLeaving = false);
	snSession* GetSessionFromToken(u64 nSessionToken, bool bAllowLeaving = false);
	SessionType GetSessionType(snSession* pSession);
	snSession* GetSessionPtr(const SessionType sessionType);

	void OnSessionEventTransition(snSession* pSession, const snEvent* pEvent);
	void OnGetGamerDataTransition(snSession* pSession, const rlGamerInfo& gamerInfo, snGetGamerData* pData) const;
	
	void ProcessAutoJoin();

    int FindGamerInStore(const rlGamerHandle& hGamer);
	void SyncRadioStations(const u64 nSessionID, const rlGamerId& nGamerID);

	void SendAddedGamerTelemetry(bool isTransitioning, const u64 sessionId, const snEventAddedGamer* pEvent, netNatType natType);
	u64 SendMatchmakingStartTelemetry(sMatchmakingQuery* pQueries, const unsigned nQueries);
	void SendMatchmakingResultsTelemetry(sMatchmakingResults* pResults);

#if RSG_XBOX
	bool m_bHasWrittenStartEvent[SessionType::ST_Max];
	void WriteMultiplayerStartEventSession(OUTPUT_ONLY(const char* szTag));
	void WriteMultiplayerEndEventSession(bool bClean OUTPUT_ONLY(, const char* szTag));
	void WriteMultiplayerStartEventTransition(OUTPUT_ONLY(const char* szTag));
	void WriteMultiplayerEndEventTransition(bool bClean OUTPUT_ONLY(, const char* szTag));
#endif

	void SetDefaultPlayerSessionData();
	unsigned GetNumBossesInFreeroam() const;

	snSession*			m_pSession;
	snSession*			m_pTransition;

#if !RSG_PRESENCE_SESSION
	netStatus			m_pSessionModifyPresenceStatus;
#endif
	netStatus*			m_pSessionStatus;
	netStatus*			m_pTransitionStatus; 
	
	snSession			m_Sessions[SessionType::ST_Max];
	netStatus			m_Status[SessionType::ST_Max];
    NetworkGameConfig	m_Config[SessionType::ST_Max];
    bool				m_bChangeAttributesPending[SessionType::ST_Max];
    netStatus           m_ChangeAttributesStatus[SessionType::ST_Max];
	unsigned            m_nHostFlags[SessionType::ST_Max];
	u64					m_nUniqueMatchmakingID[SessionType::ST_Max];
	u64					m_LastMigratedPosix[SessionType::ST_Max];

	MetricSessionMigrated m_MigratedTelemetry;
	bool				m_SendMigrationTelemetry;

	eSessionState		m_SessionState;				// The current session state
	NetworkGameFilter	m_SessionFilter;			// Used to describe settings to use when searching for a session
	unsigned			m_LastUpdateTime;			// The last time the session was updated
    bool				m_IsInitialised		: 1;	// Is the session initialised?
    unsigned			m_TimeSessionStarted;		// The time the session started 
	LeaveSessionReason m_LeaveReason;				// Reason we left the last session
    unsigned			m_LeaveSessionFlags;		// Flags when we left the last session

	unsigned			m_nSessionTimeout;				// Timeout time when getting into a session
	float				m_fMigrationTimeoutMultiplier;	// Multiplier on session state timeout when migrating
	unsigned			m_SessionStateTime;			// Tracks state time

	eBailReason			m_nBailReason;				// Keeps track of our bail reason when a frame delay through STATE_BAILING is needed

	bool				m_bJoinIncomplete;			// Join process is incomplete and the host has disconnected
	bool				m_bJoinedViaInvite;			// If a client, indicates whether we were invited (vs. joined via matchmaking)
	bool				m_bBlockJoinRequests;		// Indicates whether to block incoming join requests or not

	bool				m_bIsMigrating;				// Copy of the session variable applied in Update
	bool				m_bAutoJoining;	

    bool                m_bCheckForAnotherSession;

	bool				m_bSendHostDisconnectMsg;

	rlSessionInfo		m_PreviousSession;
    bool                m_bPreviousWasPrivate;
    unsigned            m_nPreviousMaxSlots;
	unsigned			m_nPreviousOccupiedSlots;
	int					m_PreviousGameMode;

	static const unsigned NUM_HOST_RETRIES = 1;
	unsigned			m_nHostRetries;
	unsigned			m_nHostRetriesTransition;

	SessionTypeForTelemetry m_SessionTypeForTelemetry;

#if RSG_DURANGO
	bool m_bHandlePartyActivityDialog; 
	bool m_bShowingPartyActivityDialog;
	void HandlePartyActivityDialog(); 
	bool m_bValidateGameSession;
#endif

	bool m_bCanQueue;
	bool m_bJoinQueueAsSpectator;
	bool m_bJoinQueueConsumePrivate; 
    rlGamerHandle m_hJoinQueueInviter;
    unsigned m_nJoinQueueToken;
	rlGamerHandle m_hJoinQueueGamer;
	
	void CheckForQueuedJoinRequests();
	bool RemoveQueuedJoinRequest(const rlGamerHandle& hGamer);
	void UpdateQueuedJoinRequests();
	
	// join queue - allow
	static const unsigned MAX_QUEUED_PLAYERS = 10;
	static const unsigned JOIN_QUEUE_INVITE_TIMEOUT = 30000;
	static const unsigned JOIN_QUEUE_MAX_WAIT_QUEUED_INVITE = 120000;
	static const unsigned JOIN_QUEUE_TIME_SPENT_IN_SP_TO_REMOVE = 120000;
	static const unsigned JOIN_QUEUE_INVITE_COOLDOWN = 60000;
	
	struct sQueuedJoinRequest
	{
		sQueuedJoinRequest() { Clear(); }
		void Clear() { m_hGamer.Clear(); m_bIsSpectator = false; m_nCrewID = 0; m_nUniqueToken = 0; m_nQueuedTimestamp = 0; m_nInviteSentTimestamp = 0; m_nLastInviteSentTimestamp = 0; }
		rlGamerHandle m_hGamer;
		bool m_bIsSpectator;
		bool m_bConsumePrivate;
		s64 m_nCrewID; 
		unsigned m_nUniqueToken;
		unsigned m_nQueuedTimestamp;
		unsigned m_nInviteSentTimestamp;
		unsigned m_nLastInviteSentTimestamp;
	};
	atFixedArray<sQueuedJoinRequest, MAX_QUEUED_PLAYERS> m_QueuedJoinRequests;
	unsigned m_LastCheckForJoinRequests;
	unsigned m_TimeReturnedToSinglePlayer;
	unsigned m_QueuedInviteTimeout;
	unsigned m_MaxWaitForQueuedInvite;
	unsigned m_JoinQueueInviteCooldown;

	bool m_AdminInviteNotificationRequired;
	bool m_AdminInviteKickToMakeRoom;
	bool m_AdminInviteKickIncludesLocal;

	unsigned m_nNumQuickmatchOverallResultsBeforeEarlyOut;
	unsigned m_nNumQuickmatchOverallResultsBeforeEarlyOutNonSolo;
	unsigned m_nNumQuickmatchResultsBeforeEarlyOut;
	unsigned m_nNumQuickmatchSocialResultsBeforeEarlyOut;
	unsigned m_nNumSocialResultsBeforeEarlyOut;
	bool m_bDisableDelayedMatchmakingAdvertise;
	bool m_bCancelAllTasksOnLeave;
	bool m_bCancelAllConcurrentTasksOnLeave;
	bool m_bCheckForAnotherSessionOnMigrationFail;

	bool m_bEnterMatchmakingAsBoss; 
	unsigned m_nBossRejectThreshold;
	unsigned m_nBossDeprioritiseThreshold; 
	bool m_bRejectSessionsWithLargeNumberOfBosses;

	unsigned m_IntroBossRejectThreshold;
	unsigned m_IntroBossDeprioritiseThreshold;
	bool m_IntroRejectSessionsWithLargeNumberOfBosses;

	bool m_bIsInviteProcessPendingForCommerce; 
    bool m_bIsInvitePendingJoin;

    bool m_PendingHostPhysical;

	struct sPendingJoiners
	{
		struct sPendingJoiner
		{
			sPendingJoiner() : m_JoinRequestTime(0), m_JoinFlags(0) {}
			rlGamerHandle m_hGamer;
			unsigned m_JoinRequestTime;
			unsigned m_JoinFlags;
		};

		void Add(const rlGamerHandle& joiningPlayer, const unsigned joinFlags)
		{
			// if we're full, just delete the first entry (see Remove, we use Delete and not DeleteFast to 
			// guarantee these are in time added order
			// this is possible if a session sized amount of players all join at once and one or more fail to 
			// make it. Should be rare enough that it's not worth adding a buffer
			if(m_Joiners.IsFull())
			{
				m_Joiners.Delete(0);
			}

			sPendingJoiner joiner;
			joiner.m_hGamer = joiningPlayer;
			joiner.m_JoinFlags = joinFlags;
			joiner.m_JoinRequestTime = sysTimer::GetSystemMsTime();
			m_Joiners.Push(joiner);
		}

		int Find(const rlGamerHandle& player) const
		{
			const int numJoiners = m_Joiners.GetCount();
			for(int i = 0; i < numJoiners; i++)
			{
				if(m_Joiners[i].m_hGamer == player)
					return i;
			}
			return -1;
		}

		bool Remove(const rlGamerHandle& player)
		{
			// use delete and not delete fast so that we can guarantee that joiners are in added order
			const int playerIdx = Find(player);
			if(playerIdx >= 0)
			{
				m_Joiners.Delete(playerIdx); 
				return true;  
			}
			return false;
		}

		void Reset()
		{
			m_Joiners.Reset();
		}

		atFixedArray<sPendingJoiner, RL_MAX_GAMERS_PER_SESSION> m_Joiners;
	};
	sPendingJoiners m_PendingJoiners[SessionType::ST_Max];
	unsigned m_PendingJoinerTimeout;
	bool m_PendingJoinerEnabled;

	struct sPendingReservations
	{
		sPendingReservations() : m_nGamers(0), m_nReservationTime(0) {}
		rlGamerHandle m_hGamers[RL_MAX_GAMERS_PER_SESSION];
		unsigned m_nGamers;
		unsigned m_nReservationTime;

		void Reset()
		{
			m_nGamers = 0;
		}
	};
	sPendingReservations m_PendingReservations[SessionType::ST_Max];

	bool m_bForceScriptPresenceUpdate;
	bool m_bRunningDetailQuery;
    JoinFailureAction m_JoinFailureAction;
	int m_JoinFailureGameMode;
    rlSessionDetail m_JoinSessionDetail;
	rlSessionInfo m_JoinSessionInfo;
    rlSlotType m_JoinSlotType;
	unsigned m_nJoinSessionDetail;
	netStatus m_JoinDetailStatus;
	unsigned m_nJoinFlags;
	bool m_bCanRetryJoinFailure;
	bool m_bFreeroamIsJobToJob; 

	// matchmaking
    bool m_MatchmakingQueryEnabled[MatchmakingQueryType::MQT_Num];
	MatchmakingAttributes m_MatchmakingAttributes;	
	eMatchmakingGroup m_MatchmakingGroup;
	unsigned m_MatchmakingGroupMax[MM_GROUP_MAX];
	unsigned m_nActiveMatchmakingGroups;
	eMatchmakingGroup m_ActiveMatchmakingGroups[MAX_ACTIVE_MM_GROUPS];
	u8 m_nMatchmakingPropertyID;
	u8 m_nMatchmakingMentalState;
	u32 m_nMatchmakingMinimumRank;
    u16 m_nMatchmakingELO;

    static const unsigned MATCHMAKING_GROUP_CHECK_INTERVAL = 1000;
    unsigned m_nLastMatchmakingGroupCheck;

	bool m_bIsJoiningViaMatchmaking; // determines whether a join operations was started using the find results array

	static const unsigned kMAX_SOCIAL_QUERY = 64;
	static const unsigned kMAX_SOCIAL_PARAMS = 256;
	char m_szSocialQuery[kMAX_SOCIAL_QUERY];
	char m_szSocialParams[kMAX_SOCIAL_PARAMS];

    // host settings
    s32 m_nHostAimPreference; 

	// host probability
	bool m_bEnableHostProbability;
	float m_fMatchmakingHostProbability;
	int m_nMatchmakingAttempts;
	int m_MaxMatchmakingAttempts;
	float m_fMatchmakingHostProbabilityStart;
	float m_fMatchmakingHostProbabilityScale;

    // visibility locking
    bool m_bLockVisibility[SessionType::ST_Max];
    bool m_bLockSetting[SessionType::ST_Max];

    // storage for session search results
	static const unsigned MAX_MATCHMAKING_RESULTS = 15;
	struct sMatchmakingQuery
	{
		sMatchmakingQuery() 
			: m_nResults(0)
			, m_QueryType(MatchmakingQueryType::MQT_Invalid)
			, m_bIsActive(false)
			, m_pFilter(nullptr) 
		{
			m_TaskStatus.Reset(); 
		}

		rlSessionDetail m_Results[MAX_MATCHMAKING_RESULTS];
        unsigned m_nNumHits[MAX_MATCHMAKING_RESULTS];
        bool m_bCountHits;
		unsigned m_nResults;
		netStatus m_TaskStatus; 
		bool m_bSameRegionOnly;
		bool m_bIsActive;
		bool m_bIsPending;
		MatchmakingQueryType m_QueryType;
		NetworkGameFilter* m_pFilter; 
		unsigned m_nPreferenceOrder[MAX_MATCHMAKING_RESULTS];
		u64 m_UniqueID;

		void Init(NetworkGameFilter* pFilter)
		{
			m_pFilter = pFilter;
		}
		
		void Reset(MatchmakingQueryType kQuery)
		{
			m_QueryType = kQuery;
			m_TaskStatus.Reset();
			m_nResults = 0;
            m_bCountHits = false;
			m_bIsActive = false;
			m_bIsPending = false;
			m_bSameRegionOnly = false;
			m_UniqueID = 0;

			for(unsigned i = 0; i < MAX_MATCHMAKING_RESULTS; i++)
			{
				m_nNumHits[i] = 0;
				m_nPreferenceOrder[i] = 0;
			}
		}

		bool IsSocial() { return (m_QueryType == MatchmakingQueryType::MQT_Friends || m_QueryType == MatchmakingQueryType::MQT_Social); }
		bool IsPlatform() { return (m_QueryType == MatchmakingQueryType::MQT_Standard); }
		bool IsBusy() const { return m_TaskStatus.Pending(); }
	};

	// for freeroam, we can issue one query of each type
	sMatchmakingQuery m_MatchMakingQueries[MatchmakingQueryType::MQT_Num];

	struct sMatchmakingResults
	{
		sMatchmakingResults() 
			: m_UniqueID(0)
			, m_GameMode(0)
			, m_MatchmakingRetrievedTime(0)
			, m_nCandidateToJoinIndex(0)

			, m_nNumCandidates(0)
			, m_nNumCandidatesAttempted(0)
			, m_bHosted(false) 
		{

		}
		
		// list of sessions we'll attempt to join
		u64 m_UniqueID;
        unsigned m_GameMode;
		unsigned m_SessionType;
		unsigned m_MatchmakingRetrievedTime;
		unsigned m_nCandidateToJoinIndex;
		unsigned m_nNumCandidates;
		unsigned m_nNumCandidatesAttempted;
		bool m_bHosted;

		// offset bail result codes to distinguish from join response codes
		static const unsigned BAIL_OFFSET = 0x40;
		
		struct sCandidate
		{
			rlSessionDetail m_Session;
			unsigned m_nTimeStarted; 
			unsigned m_nTimeFinished;
			unsigned m_Result; 
			float m_fScore;
			unsigned m_nFromQueries;

			void Clear()
			{
				m_Session.Clear();
				m_nTimeStarted = 0;
				m_nTimeFinished = 0;
				m_Result = 0;
				m_fScore = -1.0f;
				m_nFromQueries = 0;
			}
		};

		sCandidate m_CandidatesToJoin[NETWORK_MAX_MATCHMAKING_CANDIDATES];
	};
	sMatchmakingResults m_GameMMResults;

	unsigned m_nMatchmakingFlags;
	bool m_bTransitionViaMainMatchmaking;
    unsigned m_nMatchmakingStarted;
    unsigned m_nQuickMatchStarted;
	unsigned m_nQuickmatchFallbackMaxPlayers;

	static const unsigned DEFAULT_LARGE_CREW_VALUE = 100;
	unsigned m_nLargeCrewValue;

    // candidate boosting
    unsigned m_nMatchmakingBeaconTimer[SessionType::ST_Max];
    unsigned m_nMatchmakingBeacon[SessionType::ST_Max];
    unsigned m_nBeaconInterval;
    bool m_bUseMatchmakingBeacon;

    // region / language bias
    bool m_bEnableLanguageBias;
    bool m_bEnableRegionBias; 
    unsigned m_nLanguageBiasMask;
    unsigned m_nRegionBiasMask;
    unsigned m_nLanguageBiasMaskForRegion;
    unsigned m_nRegionBiasMaskForLanguage;

	// tunables for specific query types
	bool m_bUseRegionMatchmakingForToGame;
	bool m_bUseLanguageMatchmakingForToGame;
	bool m_bUseRegionMatchmakingForActivity;
	bool m_bUseLanguageMatchmakingForActivity;

	// sessions we've had a problem joining
	atFixedArray<rlSessionToken, NETWORK_MAX_MATCHMAKING_CANDIDATES> m_JoinFailureSessions;

    static const unsigned SESSION_USERDATA_CHECK_INTERVAL = 3000;
    unsigned m_nLastSessionUserDataRefresh;

	unsigned m_nEnterMultiplayerTimestamp;
	unsigned m_MigrationStartedTimestamp;
	unsigned m_MigrationStartPlayerCount;
	sMatchmakingInfoMine m_MatchmakingInfoMine;

#if RSG_DURANGO
	bool m_bMigrationSentMultiplayerEndEvent;
	bool m_bMigrationSentMultiplayerEndEventTransition;
#endif

    SessionUserData_Freeroam m_SessionUserDataFreeroam;
	bool m_HasDirtySessionUserDataFreeroam;

	SessionUserData_Activity m_SessionUserDataActivity;
	bool m_HasDirtySessionUserDataActivity;

	// whether script are validating the join
	bool m_bScriptValidatingJoin;
	bool m_bScriptHandlingClockAndWeather;

	// unique crew tracking
	sUniqueCrewData m_UniqueCrewData[SessionType::ST_Max];
    
    // delegates for session / connection manager event handlers
    snSessionOwner m_SessionOwner[SessionType::ST_Max];
    snSession::Delegate m_SessionDlgt[SessionType::ST_Max];
	ConnectionManagerDelegate m_ConnectionDlgtGame;
    ConnectionManagerDelegate m_ConnectionDlgtGameSession;
    ConnectionManagerDelegate m_ConnectionDlgtActivitySession;

    // flag to ignore migration failure
    bool m_bIgnoreMigrationFailure[SessionType::ST_Max];

	// session bucketing pool for this session
	eMultiplayerPool m_nSessionPool[SessionType::ST_Max];

	// if we joined via invite, stash the player who invited us
	rlGamerHandle m_Inviter;

    // peer complainer support - used to resolve broken peer mesh
	netPeerComplainer m_PeerComplainer[SessionType::ST_Max];
    netPeerComplainer::BootDelegate m_PeerComplainerDelegate;

	unsigned m_nPeerComplainerAggressiveBootDelay;
	unsigned m_nPeerComplainerBootDelay;
	bool m_bAllowComplaintsWhenEstablishing;
	bool m_bPreferToBootEstablishingCandidates;
	bool m_bPreferToBootLowestNAT;

    CNetworkRecentPlayers      m_RecentPlayers;         // Keeps track of gamers we have met during the session
    CBlacklistedGamers     m_BlacklistedGamers; // Keeps track of gamers blacklisted from joining the session
    CNetworkInvitedPlayers m_InvitedPlayers;    // Keeps track of gamers invited to join the session
	bool m_bUseBlacklist;
	bool m_bImmediateFriendSessionRefresh;

	struct BlacklistedSession
	{
		rlSessionToken m_nSessionToken;
		unsigned m_nTimestamp;
	};
	atArray<BlacklistedSession> m_BlacklistedSessions;

    // track gamers who have left for the store
    struct GamerInStore
    {
		GamerInStore()
		{
			m_hGamer.Clear();
			m_szGamerName[0] = '\0';
			m_nTimeEnteredStore = 0;
			m_nReservationTime = 0;
		}

        rlGamerHandle m_hGamer;
        char m_szGamerName[RL_MAX_NAME_BUF_SIZE];
        unsigned m_nTimeEnteredStore;
        unsigned m_nReservationTime; 
    };
    atArray<GamerInStore> m_GamerInStore;
	
	u32 m_WaitFramesSinglePlayerScriptCleanUpBeforeStart;

    bool m_bMainSyncedRadio;
    bool m_bTransitionSyncedRadio; 

	eTransitionState	m_TransitionState; 
    unsigned            m_TransitionStateTime;

	rlSessionInfo		m_TransitionJoinSessionInfo;
	rlSlotType			m_TransitionJoinSlotType;
	unsigned			m_nTransitionJoinFlags;
	bool				m_bCanRetryTransitionJoinFailure;

    bool                m_bHasSetActivitySpectatorsMax;
	unsigned			m_nActivityPlayersMax;
    unsigned            m_nActivitySpectatorsMax;
  
    static const unsigned ACTIVITY_SLOT_CHECK_INTERVAL = 1000;
    unsigned            m_nLastActivitySlotsCheck;

	static const unsigned DEFAULT_LAUNCH_COUNTDOWN = 3; 
	static const unsigned DEFAULT_IMMEDIATE_LAUNCH_COUNTDOWN = 2; 

    bool                m_bLaunchingTransition;
    bool                m_bLaunchedAsHost;
	bool				m_bTransitionStartPending;
	bool				m_bDoLaunchPending;
	int			        m_nDoLaunchCountdown;
	MsgTransitionLaunch m_sMsgLaunch;
	bool				m_bLaunchOnStartedFromJoin;
	bool				m_bAllowImmediateLaunchDuringJoin;
	bool				m_bAllowBailForLaunchedJoins;
	bool				m_bHasPendingTransitionBail;
	unsigned			m_nImmediateLaunchCountdown;
	unsigned			m_nImmediateLaunchCountdownTime;
	bool				m_bDoNotLaunchFromJoinAsMigratedHost;
    unsigned            m_TimeTokenForLaunchedTransition;

    bool                m_bIsActivitySession;
	bool				m_bIsLeavingLaunchedActivity;

	bool				m_bMigrationSessionNoEstablishedPools;
	bool				m_bMigrationTransitionNoEstablishedPools;
	bool				m_bMigrationTransitionNoEstablishedPoolsForToGame;
	bool				m_bMigrationTransitionNoSpectatorPools;
	bool				m_bMigrationTransitionNoSpectatorPoolsForToGame;
	bool				m_bMigrationTransitionNoAsyncPools;

	bool				m_bPreventStartWithTemporaryPlayers;
	bool				m_bPreventStartWhileAddingPlayers;

#if !__NO_OUTPUT
	unsigned			m_nCreateTrackingTimestamp;
	unsigned			m_nLaunchTrackingTimestamp;
#endif
    unsigned			m_nToGameTrackingTimestamp;
	u64					m_TimeCreated[SessionType::ST_Max];

	static const unsigned TRANSITION_MESSAGE_INTERVAL = 1000; 

    // player data (we keep a copy for all remote players)
    struct sTransitionPlayer
    {
        sTransitionPlayer()
        { 
			Clear(); 
		}

        sTransitionPlayer(const rlGamerHandle& hGamer, const bool bIsSpectator, rlClanId nClanID, u16 nCharacterRank)
            : m_hGamer(hGamer)
            , m_bIsSpectator(bIsSpectator) 
            , m_nClanID(nClanID)
            , m_nCharacterRank(nCharacterRank) 
			, m_nELO(0)
			, m_NatType(NET_NAT_UNKNOWN)
			, m_bIsRockstarDev(false)
			, m_bIsAsync(false)
		{
            m_bHasFullyJoinedForScript = false;
        }

		void Clear()
		{
			m_bIsSpectator = false;
			m_nClanID = RL_INVALID_CLAN_ID;
			m_nCharacterRank = 0;
			m_bHasFullyJoinedForScript = false;
			m_nELO = 0;
			m_hGamer.Clear();
			m_NatType = NET_NAT_UNKNOWN;
			m_bIsRockstarDev = false; 
			m_bIsAsync = false;
		}

        rlGamerHandle m_hGamer;
        bool m_bIsSpectator;
        rlClanId m_nClanID;
        u16 m_nCharacterRank; 
        u16 m_nELO; 
        bool m_bHasFullyJoinedForScript; 
		netNatType m_NatType;
		bool m_bIsRockstarDev;
		bool m_bIsAsync;
    };
    atFixedArray<sTransitionPlayer, MAX_NUM_PHYSICAL_PLAYERS> m_TransitionPlayers;

    // local gamer data
	struct sTransitionGamerData
	{
        sTransitionPlayer m_PlayerData;
        sTransitionParameter m_aParameters[TRANSITION_MAX_PARAMETERS];
		char m_szString[TRANSITION_MAX_STRINGS][TRANSITION_PARAMETER_STRING_MAX];
	};
	
	unsigned m_nLastTransitionMessageTime; 
	sTransitionGamerData m_TransitionData;
	bool m_bIsTransitionParameterDirty[TRANSITION_MAX_PARAMETERS];
	bool m_bIsTransitionStringDirty[TRANSITION_MAX_STRINGS]; 
	bool m_bJoinedTransitionViaInvite;
	bool m_bPreventClearTransitionWhenJoiningInvite;
	bool m_bAllAdminInvitesTreatedAsDev;
	bool m_bNoAdminInvitesTreatedAsDev;
	bool m_bAllowAdminInvitesToSolo;
	unsigned m_nTransitionInviteFlags;
	rlGamerHandle m_TransitionInviter;
	bool m_bIsTransitionAsyncWait; 
	unsigned m_nPostMigrationBubbleCheckTime;

	sMatchmakingQuery m_ActivityMMQ[ActivityMatchmakingQuery::AMQ_Num];
	NetworkGameFilter m_TransitionFilter[ActivityMatchmakingQuery::AMQ_Num];
	unsigned m_nNumActivityResultsBeforeEarlyOut;
	unsigned m_nNumActivityOverallResultsBeforeEarlyOutNonSolo;
	bool m_bActivityMatchmakingDisableAnyJobQueries;
	bool m_bActivityMatchmakingDisableInProgressQueries;
	bool m_bActivityMatchmakingContentFilterDisabled;
	bool m_bActivityMatchmakingActivityIslandDisabled;
	bool m_bJoiningTransitionViaMatchmaking;
	unsigned m_nTransitionMatchmakingFlags;
	bool m_bIsTransitionMatchmaking; 
	unsigned m_nTransitionQuickmatchFallbackMaxPlayers;
    unsigned m_nTransitionMatchmakingStarted;
	bool m_bBlockTransitionJoinRequests;

    unsigned m_nGamersToActivity;
    rlGamerHandle m_hGamersToActivity[MAX_NUM_PHYSICAL_PLAYERS];
    bool m_bRetainActivityGroupOnQuickmatchFail;
    bool m_bIsActivityGroupQuickmatch;
    rlSessionInfo m_ActivityGroupSession;
	bool m_bIsActivityGroupQuickmatchAsync; 

	sMatchmakingResults m_TransitionMMResults;
	CNetworkInvitedPlayers m_TransitionInvitedPlayers;

	bool m_bIsTransitionToGame;
	bool m_bIsTransitionToGameInSession;
	bool m_bIsTransitionToGameNotifiedSwap;
	bool m_bIsTransitionToGamePendingLeave; 
	bool m_bIsTransitionToGameFromLobby;
	int m_TransitionToGameGameMode;
    rlGamerHandle m_hTransitionHostGamer;
	rlSessionInfo m_hTransitionToGameSession;
	unsigned m_nTransitionToGameSwitchCount;
    unsigned m_nGamersToGame; 
    rlGamerHandle m_hGamersToGame[MAX_NUM_PHYSICAL_PLAYERS];
    Vector3 m_vToGamePosition;
    unsigned m_nToGameCompletedTimestamp;
	unsigned m_nTransitionClientComplaintTimeCheck;
	bool m_bTransitionToGameRemovePhysicalPlayers;

	rlGamerHandle m_hToGameManager;
	rlGamerHandle m_hActivityQuickmatchManager;

	KickReason m_LastKickReason;
    bool m_bKickCheckForAnotherSession;
	bool m_bSendRemovedFromSessionKickTelemetry;
	bool m_bKickWhenRemovedFromSession;

    rlGamerHandle m_hFollowers[RL_MAX_GAMERS_PER_SESSION];
    unsigned m_nFollowers;
    bool m_bRetainFollowers;

#if RL_NP_SUPPORT_PLAY_TOGETHER
	unsigned m_nPlayTogetherPlayers;
	rlGamerHandle m_PlayTogetherPlayers[RL_MAX_PLAY_TOGETHER_GROUP];
	bool m_bUsePlayTogetherOnFilter; 
	bool m_bUsePlayTogetherOnJoin; 
#endif

    static const int MAX_DELAYED_INVITES = 32;

    struct DelayedPresenceInviteInfo
    {
        char          m_ContentId[RLUGC_MAX_CONTENTID_CHARS];
		char          m_UniqueMatchId[MAX_UNIQUE_MATCH_ID_CHARS];
		int		      m_InviteFrom;
		int			  m_InviteType;
        unsigned      m_PlayListLength;
        unsigned      m_PlayListCurrent;
        bool          m_IsTransition;
        unsigned      m_NumDelayedInvites;
		bool		  m_bIsImportant[MAX_DELAYED_INVITES];
        rlGamerHandle m_hGamersToInvite[MAX_DELAYED_INVITES];

        void Reset()
        {
			m_ContentId[0]      = '\0';
			m_UniqueMatchId[0]  = '\0';
			m_InviteFrom		= 0;
			m_InviteType		= 0;
            m_PlayListLength    = 0;
            m_PlayListCurrent   = 0;
            m_IsTransition      = false;
            m_NumDelayedInvites = 0;
        }
    };

    DelayedPresenceInviteInfo m_DelayedPresenceInfo;

#if __BANK

public:

	void InitWidgets();

private:

	void Bank_DoQuickmatch();
	void Bank_HostSinglePlayerSession();
	void Bank_LeaveSession();
	void Bank_PrintSessionConfig();

	const CNetGamePlayer* Bank_GetChosenPlayer();
	void Bank_RefreshPlayerList();
	void Bank_KickPlayer();
	void Bank_AddPeerComplaint();
	void Bank_OpenTunnel();
	void Bank_CloseConnection();

	void Bank_RefreshFriends(); 
	void Bank_JoinFriendSession();

	void Bank_HostTransition();
	void Bank_InviteToTransition();
	void Bank_LaunchTransition();
	void Bank_LeaveTransition(); 
	void Bank_RefreshTransitionMembers();
	void Bank_JoinPlayerTransition();
	void Bank_DoTransitionMatchmaking();
	void Bank_DoTransitionMatchmakingSpecific();
	void Bank_DoTransitionMatchmakingUserContent();
	void Bank_MarkTransitionInProgress();
	void Bank_MarkTransitionUsingUserCreatedContent();

	void Bank_SendPresenceInviteToFriend();
	void Bank_SendPresenceTransitionInviteToFriend();
	void Bank_AcceptPresenceInvite();

	void Bank_ReserveSlotsAndLeave();
	void Bank_JoinPreviousSession();

	void Bank_LogSessionString();
    void Bank_ToggleMultiplayerAvailable();
    void Bank_SendTextMessage();
	void Bank_SendDataBurst();
#if RSG_PC
	void Bank_TestSimpleModDetection();
#endif

#if RSG_DURANGO
	void Bank_LogXblSessionDetails();
	void Bank_WriteXblSessionCustomProperties(); 
#endif

	char		m_BankLeaderName[RL_MAX_NAME_BUF_SIZE + 1]; // for * tagged host
	const char*	m_BankMembers[MAX_NUM_PARTY_MEMBERS];
	int			m_BankMembersComboIndex;
	const char*	m_BankFriends[rlFriendsPage::MAX_FRIEND_PAGE_SIZE];
	int			m_BankFriendsComboIndex;
	const char*	m_BankPlayers[RL_MAX_GAMERS_PER_SESSION];
	int			m_BankPlayersComboIndex;
	const char*	m_BankTransitionPlayers[RL_MAX_GAMERS_PER_SESSION];
	int			m_BankTransitionPlayersComboIndex;
    
    char		m_szSessionString[rlSessionInfo::TO_STRING_BUFFER_SIZE];

#endif // __BANK

#if !__NO_OUTPUT
	const char* Util_GetGamerLogString(const snSession* pSession, const CNetGamePlayer* player);
	const char* Util_GetGamerLogString(const snSession* pSession, const EndpointId endpointId);
	const char* Util_GetGamerLogString(const snSession* pSession, const rlGamerHandle& hGamer);
	const char* Util_GetGamerLogString(const snSession* pSession, const u64 nPeerId);
#endif
};

#endif  //NETWORKSESSION_H
