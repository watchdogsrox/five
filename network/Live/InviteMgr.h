//
// InviteMgr.h
//
// Copyright (C) 1999-2012 Rockstar Games. All Rights Reserved.
//

#ifndef INVITEMGR_H
#define INVITEMGR_H

//rage
#include "atl/array.h"
#include "rline/rlgamerinfo.h"
#include "rline/rlprivileges.h"
#include "rline/rlsession.h"
#include "rline/rlsessioninfo.h"
#include "rline/presence/rlpresencequery.h"

#include "network/NetworkTypes.h"

#if RSG_XBOX
#include "rline/durango/rlXblPrivacySettings_interface.h"
#endif

namespace rage
{
	class rlPresenceEventInviteAccepted;
	class rlPresenceEventJoinedViaPresence;
}

enum InviteFlags
{
	IF_None					= 0,
	IF_IsPlatform			= BIT0,
	IF_IsJoin				= BIT1,
	IF_ViaParty				= BIT2,
	IF_Rockstar				= BIT3,
	IF_Spectator			= BIT4, 
    IF_AllowPrivate			= BIT5,
    IF_IsRockstarInactive	= BIT6,
    IF_IsFollow				= BIT7,
	IF_PlatformParty		= BIT8,
	IF_PlatformPartyBoot	= BIT9,
	IF_PlatformPartyJoin	= BIT10,
	IF_PlatformPartyJvP		= BIT11,
	IF_ViaJoinQueue			= BIT12,
	IF_ViaAdmin				= BIT13,
	IF_IsRockstarDev		= BIT14,
	IF_NotTargeted			= BIT15,
	IF_IsTournament			= BIT16,
	IF_BadReputation		= BIT17,	// XBO only
	IF_IsBoot				= BIT18,
	IF_AutoAccept			= BIT19,

	// none of these are valid for receiving via a Rockstar / presence invite
	IF_InvalidPeerFlags	= 
		IF_IsPlatform |
		IF_ViaParty |
		IF_PlatformParty |
		IF_PlatformPartyBoot |
		IF_PlatformPartyJoin |
		IF_PlatformPartyJvP |
		IF_ViaAdmin |
		IF_IsRockstarDev |
		IF_IsTournament,
};

// local flags related to processing, etc.
enum InternalFlags
{
	LIF_None				= 0,
	LIF_FromBoot			= BIT0,
	LIF_CompletedQuery		= BIT1,
	LIF_ValidConfig			= BIT2,
	LIF_Incompatible		= BIT3,
	LIF_ProcessedConfig		= BIT4,
	LIF_Accepted			= BIT5,
	LIF_AutoConfirm			= BIT6,
	LIF_Confirmed			= BIT7,
	LIF_Actioned			= BIT8,
	LIF_KnownToScript		= BIT9,
};

enum InviteResponseFlags
{
	IRF_None								= 0,
	IRF_StatusInSinglePlayer				= BIT1,
	IRF_StatusInFreeroam					= BIT2,
	IRF_StatusInLobby						= BIT3,
	IRF_StatusInActivity					= BIT4,
	IRF_RejectCannotAccessMultiplayer		= BIT5,
	IRF_RejectNoPrivilege					= BIT6,
	IRF_RejectBlocked						= BIT7,
	IRF_RejectBlockedInactive				= BIT8,
	IRF_StatusCheater						= BIT9,
	IRF_StatusBadSport						= BIT10,
	IRF_RejectCannotProcessRockstar			= BIT11,
	IRF_StatusInStore                  	 	= BIT12,
	IRF_StatusInCreator                 	= BIT13,
	IRF_StatusInactive						= BIT14,
	IRF_StatusFromJoinQueue					= BIT15,
	IRF_StatusInSession						= BIT16,
	IRF_RejectIncompatible					= BIT17,
	IRF_StatusInEditor						= BIT18,
	IRF_CannotProcessAdmin					= BIT19,
	IRF_RejectReplacedByNewerInvite 		= BIT20,
	IRF_RejectAlreadyInSession 				= BIT21,
	IRF_RejectNotCompletedSpPrologue 		= BIT22,
	IRF_RejectNoMultiplayerCharacter 		= BIT23,
	IRF_RejectNotCompletedMultiplayerIntro 	= BIT24,
	IRF_RejectInvalidInviter 				= BIT25,

	// mask for blocked reasons (due to privileges / restrictions)
	IRF_RejectBlockedMask = 
		IRF_RejectCannotAccessMultiplayer |
		IRF_RejectNoPrivilege |
		IRF_RejectBlocked |
		IRF_RejectBlockedInactive |
		IRF_RejectIncompatible |
		IRF_CannotProcessAdmin,
};

enum InviteAction
{
	IA_None,
	IA_Accepted,
	IA_Ignored,
	IA_Rejected,
	IA_Blocked,
	IA_NotRelevant,
	IA_Max,
};

enum ResolveFlags
{
	RF_None = 0,
	RF_SendReply = BIT0,
};

typedef unsigned InviteId;
#define INVALID_INVITE_ID (0)

//PURPOSE
//  Contains data associated with an accepted platform (XBL / PSN / SC) invite
struct sPlatformInvite
{
	sPlatformInvite() { Clear(); }

	void Init(
		const rlGamerInfo& hInvitee,
		const rlGamerHandle& hInviter,
		const rlSessionInfo& sessionInfo,
		const unsigned nInviteFlags);

	void Clear();

	bool IsValid() const;

	rlGamerInfo m_Invitee;
	rlGamerHandle m_Inviter;
	rlSessionInfo m_SessionInfo; 
	InviteId m_InviteId;
	unsigned m_Flags;
	unsigned m_TimeAccepted;
};

//PURPOSE
//  Describes our custom Rockstar invite
class sRsInvite
{
	friend class CLiveManager;
	friend class InviteMgr;

private:

	enum
	{
		MAX_RS_INVITES = 8,
	};

public:

	enum
	{
		DEFAULT_RS_INVITE_TIMEOUT = 3 * 60 * 1000, // milliseconds
		DEFAULT_RS_REPLY_TTL = 1 * 60 * 1000, // milliseconds
	};

	sRsInvite();

	bool Init(
		const rlGamerInfo& hInfo,
		const rlSessionInfo& hSessionInfo,
		const rlGamerHandle& hFrom,
		const char* szGamerName,
		const InviteId nInviteId,
		const int nGameMode,
		unsigned nCrewId,
		const char* szContentId,
		const char* pUniqueMatchId,
		const int nInviteFrom,
		const int nInviteType,
		const unsigned nPlaylistLength,
		const unsigned nPlaylistCurrent,
		const unsigned nFlags);

	bool InitAdmin(const rlGamerInfo& hInfo, const rlSessionInfo& hSessionInfo, const rlGamerHandle& hSessionHost, const unsigned inviteFlags, const InviteId inviteId);
	bool InitTournament(const rlGamerInfo& hInfo, const rlSessionInfo& hSessionInfo, const rlGamerHandle& hSessionHost, const char* szContentId, unsigned nTournamentEventId, unsigned nFlags, int nInviteId);
	bool IsValid() const;

	bool IsSCTV() const { return (m_InviteFlags & InviteFlags::IF_Spectator) != 0; }
	bool IsAdmin() const { return (m_InviteFlags & InviteFlags::IF_ViaAdmin) != 0; }
	bool IsAdminDev() const { return (m_InviteFlags & InviteFlags::IF_IsRockstarDev) != 0; }
	bool IsTournament() const { return (m_InviteFlags & InviteFlags::IF_IsTournament) != 0; }
	bool RequireAck() const { return (m_InviteFlags & InviteFlags::IF_NotTargeted) == 0; }

private:

	void Clear();

	rlSessionInfo m_SessionInfo;
	rlGamerInfo m_Invitee;
	rlGamerHandle m_Inviter;
	char m_InviterName[RL_MAX_DISPLAY_NAME_BUF_SIZE];

	InviteId m_InviteId;

	int m_GameMode;
	unsigned m_CrewId;
	char m_szContentId[UGC_CONTENT_ID_MAX];
	unsigned m_nPlaylistLength;
	unsigned m_nPlaylistCurrent;
	unsigned m_TournamentEventId;
	unsigned m_InviteFlags;
	unsigned m_TimeAdded;

	char m_UniqueMatchId[MAX_UNIQUE_MATCH_ID_CHARS];
	int m_InviteFrom;
	int m_InviteType;

#if RSG_XBOX
	bool m_bAcknowledgedPermissionResult;
	struct PermissionCheck
	{
		PermissionCheck() : m_bCheckRequired(false) {}
		bool m_bCheckRequired;
		netStatus m_Status;
		PrivacySettingsResult m_Result;
	};
	PermissionCheck m_MultiplayerCheck;
	PermissionCheck m_ContentCheck;
#endif
};

//PURPOSE
//  This represents an accepted / active invite
class sAcceptedInvite
{
	friend class InviteMgr;

private:

	// invite information
	rlSessionInfo m_SessionInfo;
	rlGamerInfo m_Invitee;
	rlGamerHandle m_Inviter;
	unsigned m_InviteFlags;
	unsigned m_InternalFlags;

	enum State
	{
		STATE_IDLE,
		STATE_CHECK_ONLINE_STATUS,
		STATE_CHECK_SYSTEM_UI,
		STATE_QUERY_SESSION,
		STATE_QUERYING_SESSION,
		STATE_QUERY_PRESENCE,
		STATE_QUERYING_PRESENCE,
#if RSG_XBOX
		STATE_CHECKING_CONTENT_PERMISSIONS,
#endif
	} m_State;

#if RSG_XBOX
	netStatus m_UserContentStatus; 
	PrivacySettingsResult m_UserContentResult;
#endif

	InviteId m_InviteId;

	char m_UniqueMatchId[MAX_UNIQUE_MATCH_ID_CHARS];
	int m_InviteFrom;
	int m_InviteType; 

    static const unsigned MAX_LOCALISATION_STRING = 64;
    char m_szErrorLocalisationId[MAX_LOCALISATION_STRING]; 

	static const unsigned QUERY_RETRY_PRESENCE_WAIT_MS = 5000;
	static const unsigned QUERY_RETRY_DETAILS_WAIT_MS = 2000;
	static const unsigned QUERY_RETRY_MAX = 3;
	unsigned m_QueryNumRetries;
	int m_QueryRetryTimer;
	rlSessionQueryData m_PresenceInfo;

	// we find these out through config retrieval
	unsigned m_nGameMode;
	SessionPurpose m_SessionPurpose;
	unsigned m_nAimType; 
	int m_nActivityType;
	unsigned m_nActivityID;
	u8 m_nNumBosses;

	// session setup
	rlSessionDetail	m_SessionDetail;
    unsigned m_NumDetails;

	// task inspection
	netStatus m_TaskStatus;

	bool GetConfigDetails();

public:

	sAcceptedInvite();

	bool Init(const rlSessionInfo& sessionInfo,
              const rlGamerInfo& invitee,
              const rlGamerHandle& inviter,
			  const InviteId nInviteId, 
			  const unsigned nInviteFlags,
			  const unsigned nInternalFlags,
			  const int nGameMode,
			  const char* pUniqueMatchId,
			  const int nInviteFrom,
			  const int nInviteType);

	void Shutdown();

	void Clear();
	void Cancel(); 

	void Update(const int timeStep);

	bool IsPending() const;
	bool IsProcessing() const;
	bool HasFinished() const;
	bool HasSucceeded() const;
	bool HasFailed() const;

    bool HasConfig() const							{ return HasInternalFlag(InternalFlags::LIF_CompletedQuery); }
    bool HasValidConfig() const						{ return HasInternalFlag(InternalFlags::LIF_ValidConfig); }
    bool IsIncompatible() const						{ return HasInternalFlag(InternalFlags::LIF_Incompatible); }

    const char* GetErrorLocalisationId() const		{ return m_szErrorLocalisationId; };

    const rlSessionDetail& GetSessionDetail() const	{ return m_SessionDetail; }
    const rlSessionInfo& GetSessionInfo() const		{ return m_SessionDetail.IsValid() ? m_SessionDetail.m_SessionInfo : m_SessionInfo; }
	const rlGamerInfo& GetInvitee() const			{ return m_Invitee; }
	const rlGamerHandle& GetInviter() const			{ return m_Inviter; }
	unsigned GetGameMode() const					{ return m_nGameMode; }
	SessionPurpose GetSessionPurpose() const		{ return m_SessionPurpose; }

	const char* GetUniqueMatchId() const			{ return m_UniqueMatchId;  }
	int GetInviteFrom() const						{ return m_InviteFrom; }
	int GetInviteType() const						{ return m_InviteType; }

	InviteId GetInviteId() const					{ return m_InviteId; }
    unsigned GetFlags() const						{ return m_InviteFlags; }
    unsigned GetInternalFlags() const				{ return m_InternalFlags; }

	bool HasInternalFlag(const unsigned flag) const { return (m_InternalFlags & flag) == flag; }
	void AddInternalFlag(const unsigned flag);
	void RemoveInternalFlag(const unsigned flag);

    bool IsPlatform() const							{ return (m_InviteFlags & InviteFlags::IF_IsPlatform) != 0; }
	bool IsJVP() const								{ return (m_InviteFlags & InviteFlags::IF_IsJoin) != 0; }
	bool IsViaParty() const							{ return (m_InviteFlags & InviteFlags::IF_ViaParty) != 0; }
	bool IsRockstar() const							{ return (m_InviteFlags & InviteFlags::IF_Rockstar) != 0; }
    bool IsRockstarToInactive() const			    { return (m_InviteFlags & InviteFlags::IF_IsRockstarInactive) != 0; }
    bool IsSCTV() const								{ return (m_InviteFlags & InviteFlags::IF_Spectator) != 0; }
    bool IsPrivate() const							{ return (m_InviteFlags & InviteFlags::IF_AllowPrivate) != 0; }
    bool IsFollow() const							{ return (m_InviteFlags & InviteFlags::IF_IsFollow) != 0; }
	bool IsViaPlatformParty() const					{ return (m_InviteFlags & InviteFlags::IF_PlatformParty) != 0; }
	bool IsViaPlatformPartyBoot() const				{ return (m_InviteFlags & InviteFlags::IF_PlatformPartyBoot) != 0; }
	bool IsViaPlatformPartyJoin() const				{ return (m_InviteFlags & InviteFlags::IF_PlatformPartyJoin) != 0; }
	bool IsViaPlatformPartyJVP() const				{ return (m_InviteFlags & InviteFlags::IF_PlatformPartyJvP) != 0; }
	bool IsViaPlatformPartyAny() const				{ return IsViaPlatformParty() || IsViaPlatformPartyBoot() || IsViaPlatformPartyJoin() || IsViaPlatformPartyJVP(); }
	bool IsFromJoinQueue() const					{ return (m_InviteFlags & InviteFlags::IF_ViaJoinQueue) != 0; }
	bool IsViaAdmin() const							{ return (m_InviteFlags & InviteFlags::IF_ViaAdmin) != 0; }
	bool IsAdminDev() const							{ return (m_InviteFlags & InviteFlags::IF_IsRockstarDev) != 0; }

	static bool AreFlagsViaPlatformPartyAny(unsigned nFlags)	{ return ((nFlags & InviteFlags::IF_PlatformParty) != 0) || 
																		 ((nFlags & InviteFlags::IF_PlatformPartyBoot) != 0) ||
																		 ((nFlags & InviteFlags::IF_PlatformPartyJoin) != 0) ||
																		 ((nFlags & InviteFlags::IF_PlatformPartyJvP) != 0); }
};

//PURPOSE
// Describes an invitation we've received but have not yet accepted.
class UnacceptedInvite
{
	friend class CLiveManager;
	friend class InviteMgr;

private:

	enum
	{
		MAX_UNACCEPTED_INVITES = 8,
		UNACCEPTED_INVITE_TIMEOUT = 5 * 60 * 1000, // milliseconds
	};

public:
	static const unsigned int RL_MAX_NAME_CHARS = RL_MAX_NAME_LENGTH;
	static const unsigned int RL_MAX_INVITE_SALUTATION_CHARS = 128;

	UnacceptedInvite();

	bool IsValid() const;

	rlSessionInfo m_SessionInfo;
	rlGamerHandle m_Inviter;
	char m_InviterName[RL_MAX_NAME_LENGTH];
	char m_Salutation[RL_MAX_INVITE_SALUTATION_CHARS];
	unsigned m_Id;

private:

	void Clear();
	void Init(const rlSessionInfo& sessionInfo, const rlGamerHandle& inviter, const char* inviterName, const char* salutation);
	void Reset(const rlSessionInfo& sessionInfo, const rlGamerHandle& inviter, const char* inviterName, const char* salutation);
	void Update(const int timeStep);
	bool TimedOut() const;

	static unsigned ms_IdCount;
	int m_Timeout;

	inlist_node<UnacceptedInvite> m_ListLink;
};

//PURPOSE
// Class to manage invites.
class InviteMgr
{
	friend class CLiveManager;
	friend class sAcceptedInvite;

public:

    static const unsigned PSFP_FM_INTRO_CAN_ACCEPT_INVITES = BIT1 + BIT5; // script USED to set BIT5, but now they set a different bit. Gotta support both values, though, just in case

	enum RemoveReason
	{
		RR_TimedOut,
		RR_Oldest,
		RR_Cancelled,
		RR_Accepted,
		RR_Script,
		RR_Blocked,
	};

	enum ClearFlags
	{
		CF_Default = 0,
		CF_Force = BIT0,
		CF_FromScript = BIT1,
	};

	 InviteMgr();
	~InviteMgr();

	void Init();
	void Shutdown();
	void Update(const int timeStep);

	void ActionInvite(); 
	void ClearAll();
	void ClearInvite(const ClearFlags clearFlags = ClearFlags::CF_Default);
    void CancelInvite(); 
    void ClearUnaccepted();

	// commands script can call to control invite flow
	void SuppressProcess(bool bSuppress);
	bool IsSuppressed() { return m_bSuppressProcess; }
	void BlockInvitesFromScript(bool bBlocked);
	void StoreInviteThroughRestart();
	void AllowProcessInPlayerSwitch(bool bAllow);
	void BlockJoinQueueInvites(bool bBlocked);
	void SetCanReceiveRsInvites(const bool canReceive);

	// accepted invite checks
	sAcceptedInvite* GetAcceptedInvite();

	bool HasPendingAcceptedInvite() const			{ return m_AcceptedInvite.IsPending(); }
	bool HasFailedAcceptedInvite() const			{ return m_AcceptedInvite.HasFailed(); }
	bool HasDetailsForAcceptedInvite() const		{ return m_AcceptedInvite.HasInternalFlag(InternalFlags::LIF_ProcessedConfig); }
	bool HasActionableAcceptedInvite() const		{ return m_AcceptedInvite.HasInternalFlag(InternalFlags::LIF_Accepted) || m_AcceptedInvite.HasInternalFlag(InternalFlags::LIF_Confirmed); }
	bool HasConfirmedAcceptedInvite() const			{ return m_AcceptedInvite.HasInternalFlag(InternalFlags::LIF_Confirmed); }
	
	bool HasConfirmedOrIsAutoConfirm() const		{ return HasConfirmedAcceptedInvite() || m_AcceptedInvite.HasInternalFlag(InternalFlags::LIF_AutoConfirm); }
	bool IsAwaitingInviteConfirm() const			{ return HasDetailsForAcceptedInvite() && !HasConfirmedAcceptedInvite(); }
	bool IsAwaitingInviteResponse() const;

	bool IsAcceptedInviteFromBoot() const			{ return m_AcceptedInvite.HasInternalFlag(InternalFlags::LIF_FromBoot); }
	bool IsAcceptedInviteForDifferentUser() const;

	bool IsDisplayingConfirmation()	const			{ return IsConfirmationAlert(m_ShownAlertReason) || m_ShowingConfirmationExternally; }
	void SetShowingConfirmationExternally();

	void AutoConfirmInvite();
    bool RequestConfirmEvent();

    // platform invites
    bool HasBootInviteToProcess(OUTPUT_ONLY(bool logresult = false));
	bool DidSkipBootUi() const;
    bool SignInInviteeForBoot();

	// native events
	void HandleEventPlatformInviteAccepted(const rlPresenceEventInviteAccepted* pEvent);
	void HandleEventPlatformJoinViaPresence(const rlPresenceEventJoinedViaPresence* pEvent);

	// custom / game events
	void HandleInviteFromJoinQueue(const rlSessionInfo& hInfo, const rlGamerInfo& hInvitee, const rlGamerHandle& hInviter, const InviteId nInviteId, const unsigned nJoinQueueToken, unsigned nInviteFlags);
	void HandleJoinRequest(const rlSessionInfo& hInfo, const rlGamerInfo& hInvitee, const rlGamerHandle& hInviter, unsigned inviteFlags);
	
	void AcceptPresenceInvite(
		const rlPresenceEventInviteAccepted* pEvent, 
		const InviteId nInviteId, 
		const unsigned nInviteFlags,
		const int nGameMode,
		const char* pUniqueMatchId,
		const int nInviteFrom,
		const int nInviteType);

	void HandleEventInviteUnavailable(const rlPresenceEventInviteUnavailable* pEvent);

#if RSG_XDK
	void HandleEventGameSessionReady(const rlPresenceEventGameSessionReady* pEvent, const unsigned nInviteFlags);
#endif

	bool AddRsInvite(
		const rlGamerInfo& hInvitee,
		const rlSessionInfo& hSessionInfo,
		const rlGamerHandle& hFrom, 
		const char* szGamerName,
		const unsigned nStaticDiscriminatorEarly,
		const unsigned nStaticDiscriminatorInGame,
		const InviteId nInviteId,
		const int nGameMode,
		unsigned nCrewId,
		const char* szContentId,
		const rlGamerHandle& hContentCreator, 
		const char* pUniqueMatchId,
		const int nInviteFrom,
		const int nInviteType,
		const unsigned nPlaylistLength,
		const unsigned nPlaylistCurrent,
		const unsigned nInviteFlags);

	bool OnReceivedAdminInvite(
		const rlGamerInfo& hInvitee,
		const rlSessionInfo& hSessionInfo,
		const rlGamerHandle& hSessionHost,
		const unsigned inviteFlags);

	bool OnReceivedTournamentInvite(const rlGamerInfo& hInvitee, 
		const rlSessionInfo& hSessionInfo, 
		const rlGamerHandle& hSessionHost, 
		const char* szContentId, 
		const unsigned nTournamentEventId, 
		const unsigned inviteFlags);

    bool CancelRsInvite(const rlSessionInfo& hSessionInfo, const rlGamerHandle& hFrom); 

    bool OnInviteResponseReceived(
		const rlGamerHandle& hFrom, 
		const rlSessionToken& sessionToken, 
		const InviteId nInviteId, 
		const unsigned responseFlags, 
		const unsigned inviteFlags, 
		const int nCharacterRank, 
		const char* szClanTag, 
		const bool bDecisionMade, 
		const bool bDecision);

	bool NotifyPlatformInviteAccepted(sAcceptedInvite* pInvite);

	// Platform (rlXbl / rlNp / SC) invites
	bool HasPendingPlatformNativeInvite() const;	// if an invite is currently processing at rlXbl / rlNp level
	bool HasPendingPlatformInvite() const;
	bool IsPlatformInviteJvP() const;
	const rlGamerHandle& GetPlatformInviteInviter() const;
	InviteId GetPlatformInviteId() const;
	unsigned GetPlatformInviteFlags() const;
	bool ActionPlatformInvite();
	void ClearPlatformInvite();
	void OnSetCredentialsResult(const bool bHasResult, const bool bResult);

	unsigned GetNumUnacceptedInvites();
	int GetUnacceptedInviteIndex(const rlGamerHandle& inviter);
	const UnacceptedInvite* GetUnacceptedInvite(const int idx);
	bool AcceptInvite(const int idx);
	void RemoveMatchingUnacceptedInvites(const rlSessionInfo& sessionInfo, const rlGamerHandle& inviter);
	void RemoveMatchingUnacceptedInvites(const rlGamerHandle& inviter);
	void RemoveMatchingUnacceptedInvites(const rlSessionInfo& sessionInfo);

	// Rockstar invites
	int GetRsInviteIndexByInviteId(const InviteId nInviteId);
	unsigned GetNumRsInvites();
	bool AcceptRsInvite(const unsigned inviteIdx);  
	bool RemoveRsInvite(const unsigned inviteIdx, const RemoveReason removeRe);  
    void FlushRsInvites(const rlSessionInfo& hSessionInfo);
    unsigned GetRsInviteId(const unsigned inviteIdx);
    const char* GetRsInviteInviter(const unsigned inviteIdx);
	unsigned GetRsInviteInviterCrewId(const unsigned inviteIdx);
	rlGamerHandle* GetRsInviteHandle(const unsigned inviteIdx, rlGamerHandle* pHandle);
    rlSessionInfo* GetRsInviteSessionInfo(const unsigned inviteIdx, rlSessionInfo* pInfo);
    const char* GetRsInviteContentId(const unsigned inviteIdx);
	unsigned GetRsInvitePlaylistLength(const unsigned inviteIdx);
	unsigned GetRsInvitePlaylistCurrent(const unsigned inviteIdx);
    unsigned GetRsInviteTournamentEventId(const unsigned inviteIdx);
    bool GetRsInviteScTv(const unsigned inviteIdx);
    bool GetRsInviteFromAdmin(const unsigned inviteIdx);
    bool GetRsInviteIsTournament(const unsigned inviteIdx);

    void SetFollowInvite(const rlSessionInfo& hInfo, const rlGamerHandle& hFrom);
    bool HasValidFollowInvite(); 
    const rlSessionInfo& GetFollowInviteSessionInfo();
    const rlGamerHandle& GetFollowInviteHandle();
    bool ActionFollowInvite();
    bool ClearFollowInvite();

	const char* GetConfirmText();
	const char* GetConfirmSubstring();

	void AddAdminInviteNotification(const rlGamerHandle& invitedGamer); 
	bool HasAdminInviteNotification(const rlGamerHandle& invitedGamer);

	void OnTunablesRead();

	static InviteId GenerateInviteId();

private:

	enum InviteAlertReason
	{
		IAR_None = 0,

		// confirmation
		IAR_ConfirmInvite,
		IAR_ConfirmParty,
		IAR_ConfirmNonParty,
		IAR_ConfirmViaParty,

		// failure
		IAR_InvalidContent,
		IAR_SessionError,
		IAR_PartyError,				// XBO
		IAR_AlreadyInSession,
		IAR_ActiveHost,
		IAR_Underage,
		IAR_Incompatibility,
		IAR_CannotAccess,
		IAR_NoOnlinePrivileges,
		IAR_NotOnline,
		IAR_VideoEditor,
		IAR_VideoDirector,
		IAR_NoOnlinePermissions,
		IAR_NoMultiplayerCharacter,
		IAR_NotCompletedMultiplayerIntro,
		IAR_NotCompletedSpIntro,

		// max
		IAR_Max
	};

	bool IsConfirmationAlert(const InviteAlertReason alertReason) const
	{ 
		return
			alertReason == IAR_ConfirmInvite ||
			alertReason == IAR_ConfirmParty ||
			alertReason == IAR_ConfirmNonParty ||
			alertReason == IAR_ConfirmViaParty;
	}

	bool DidInviteFlowShowAlert() const;
	bool IsAlertActive() const;
	bool IsNonInviteAlertActive() const { return IsAlertActive() && !DidInviteFlowShowAlert(); }

	bool DoesUiRequirePlatformInvite();

	enum PlatformInviteCheckResult
	{
		Result_NotReady,
		Result_Succeeded,
		Result_Failed,
	};

	PlatformInviteCheckResult CheckAndFlagPlatformInviteAlerts();
	void AddPlatformInvite(const rlGamerInfo& hInvitee, const rlGamerHandle& hInviter, const rlSessionInfo& sessionInfo, unsigned inviteFlags);
	void UpdatePendingPlatformFlowState();
	void TryProcessPlatformInvite(OUTPUT_ONLY(const char* szSource));
	bool ShouldPlatformInviteCheckStats() const; 

	// begins the process of pulling session details, etc.
    void AcceptInvite(
		const rlSessionInfo& hInfo, 
		const rlGamerInfo& hInvitee, 
		const rlGamerHandle& hInviter, 
		const InviteId nInviteId, 
		const unsigned nInviteFlags,
		const int nGameMode,
		const char* pUniqueMatchId, 
		const int nInviteFrom, 
		const int nInviteType);

	void ResolveInvite(const sRsInvite& rsInvite, const InviteAction inviteAction, const unsigned nResolveFlags, const unsigned nResponseFlags);
	void ResolveInvite(sAcceptedInvite* pInviteInfo, const InviteAction inviteAction, const unsigned nResolveFlags, const unsigned nResponseFlags);
	void ResolveInvite(sPlatformInvite& platformInvite, const InviteAction inviteAction, const unsigned nResolveFlags, const unsigned nResponseFlags);
	void ResolveInvite(
		const rlGamerInfo& hInvitee,
		const rlGamerHandle& hFrom,
		const rlSessionInfo& hSessionInfo,
		const InviteAction inviteAction,
		const unsigned nResolveFlags,
		const unsigned nResponseFlags,
		const unsigned nInviteFlags,
		const InviteId inviteId,
		const int gameMode,
		const char* pUniqueMatchId,
		const int inviteFrom,
		const int inviteType);

	void SendInviteResponse(const rlGamerInfo& hInvitee, const rlGamerHandle& hFrom, const rlSessionToken& sessionToken, const InviteId nInviteId, bool* bDecision, unsigned responseFlags, unsigned inviteFlags);

    void ConfirmInvite(); 
	void CheckInviteIsValid();
    
	bool CheckForInviteErrors();
	
	bool CheckAndMakeRoomForRsInvite();
	unsigned ValidateRsInvite(const rlGamerHandle& hGamer, const rlClanId clanId, const unsigned inviteFlags);
    
	bool UpdateAndCheckInviteBlocker(const bool isSuppressed);
	void UpdateAcceptedInvite(const int timeStep);
	bool IsProcessingSuppressed();
	bool CanGenerateScriptEvents();
	void ShowPendingAlert();

	// alert screen handling
	InviteAlertReason m_PendingAlertReason;
	InviteAlertReason m_ShownAlertReason;
	InviteAlertReason m_LastShownAlertReason;
	bool m_ShowingConfirmationExternally;

	bool m_bInviteUnavailablePending;
	bool m_PendingNetworkAccessAlert;

	bool m_bSuppressProcess; 
    bool m_bBlockedByScript;
    bool m_bStoreInviteThroughRestart; 
    bool m_bConfirmedDifferentUser;
	bool m_bWaitingForScriptDeath;
    bool m_bBusySpinnerActivated;
    bool m_bAllowProcessInPlayerSwitch;
	bool m_bBlockJoinQueueInvite;
	bool m_CanReceiveRsInvites;
	bool m_WasBlockedLastCheck;

	sAcceptedInvite m_AcceptedInvite;
	sAcceptedInvite m_PendingAcceptedInvite; 

	enum PendingPlatformInviteFlowState
	{
		FS_Inactive,
		FS_WaitingForRequiredUi,
		FS_WaitingForAccessChecks,
		FS_WaitingForScript,
	};

	sPlatformInvite m_PendingPlatformInvite;
	PendingPlatformInviteFlowState m_PendingPlatformInviteFlowState;
	bool m_IndicatedPlatformInviteAction;
	bool m_PendingPlatformScriptEventCheck;
	bool m_HasClaimedBootInvite;
	bool m_HasSkipBootUiResult;
	bool m_DidSkipBootUi;

	atFixedArray<sRsInvite, sRsInvite::MAX_RS_INVITES> m_RsInvites;
	atArray<UnacceptedInvite> m_UnacceptedInvites;

    rlSessionInfo m_FollowInviteSessionInfo;
    rlGamerHandle m_FollowInviteGamerHandle; 

	unsigned m_RsInviteAllowedGroups;

	unsigned m_RsInviteTimeout;
	unsigned m_RsInviteReplyTTL;
	bool m_bSendRsInviteAcks;
	bool m_bSendRsInviteBusyForActivities;
	bool m_bAllowAdminInviteProcessing;

    static const unsigned MAX_LOCALISATION_STRING = 64;
    char m_szFailedScreenSubstring[MAX_LOCALISATION_STRING];
    int m_AlertScreenNumber;

	static const unsigned MAX_NUM_ADMIN_INVITES_TO_TRACK = 32;
	atFixedArray<rlGamerHandle, MAX_NUM_ADMIN_INVITES_TO_TRACK> m_AdminInvitedGamers;

private:

	bool ShowAlert(const InviteAlertReason alertReason, int* numberToInsert, bool* pReturnSelection);

	void UpdateFlags();
	void UpdateRsInvites();

	bool CanClearInvite(const ClearFlags clearFlags);

	void AddUnacceptedInvite(const rlSessionInfo& sessionInfo, const rlGamerHandle& inviter, const char* inviterName, const char* salutation);
	void RemoveUnacceptedInvite(const unsigned idx);
	void UpdateUnacceptedInvites(const int timeStep);
	void SortUnacceptedInvites();
	int GetMostRecentUnacceptedInviteIndex(const rlSessionInfo& sessionInfo);
};

#endif // INVITEMGR_H

// eof

