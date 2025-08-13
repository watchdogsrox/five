//
// name:        NetworkSessionUtils.h
// description: Utility classes used by the session management code in network match
// written by:  Daniel Yelland
//

#ifndef NETWORK_SESSION_UTILS_H
#define NETWORK_SESSION_UTILS_H

#include "atl/inlist.h"
#include "atl/array.h"
#include "net/message.h"
#include "rline/rl.h"
#include "rline/rlgamerinfo.h"

#include "network/NetworkTypes.h"

namespace rage
{
    class rlGamerHandle;
    class rlGamerId;
    class rlGamerInfo;
}

// Action to take if a join attempt fails
enum JoinFailureAction
{
	JFA_None,
	JFA_Quickmatch,
	JFA_HostPublic,
	JFA_HostPrivate,
	JFA_HostSolo,
	JFA_Max,
};

// Type of query that we will request from the server
enum MatchmakingQueryType
{
	MQT_Invalid = -1,
	MQT_Standard,
	MQT_Social,
	MQT_Friends,
	MQT_Num,
};

// Activity queries are simultaneous queries we can issue for activity matchmaking results
// with different sets of parameters
enum ActivityMatchmakingQuery
{
	AMQ_Invalid = -1,
	AMQ_SpecificJob,
	AMQ_AnyJob,
	AMQ_InProgressSpecificJob,
	AMQ_InProgressAnyJob,
	AMQ_Num,
};

// matchmaking flags
enum MatchmakingFlags
{
	MMF_Asynchronous = BIT0,
	MMF_Quickmatch = BIT1,
	MMF_AllowBlacklisted = BIT2,
	MMF_AnySessionType = BIT3,
	MMF_SameRegionOnly = BIT4,
	MMF_SortInQueryOrder = BIT5,
	MMF_DisableAnyJobQueries = BIT6,		//< jobs only
	MMF_DisableInProgressQueries = BIT7,		//< jobs only
	MMF_QueryOnly = BIT8,
	MMF_RockstarCreatedOnly = BIT9,		//< jobs only
	MMF_UserCreatedOnly = BIT10,	//< jobs only
	MMF_IsBoss = BIT11,
	MMF_JobToJob = BIT12,	//< to game only
	MMF_NoScriptEventOnBail = BIT13,
	MMF_BailFromLaunchedJobs = BIT14,
	MMF_InProgressOnly = BIT15,    //< jobs only
	MMF_ToGameViaTransiton = BIT16,    //< to game only
	MMF_ExpandedIntroFlow = BIT17,
	MMF_Default = 0,
};

enum HostFlags
{
	HF_IsPrivate = BIT0,
	HF_NoMatchmaking = BIT1,
	HF_SingleplayerOnly = BIT2,
	HF_Closed = BIT3,
	HF_ClosedFriends = BIT4,
	HF_ClosedCrew = BIT5,
	HF_AllowPrivate = BIT6,
	HF_ViaMatchmaking = BIT7,
	HF_ViaScript = BIT8,
	HF_ViaJoinFailure = BIT9,
	HF_ViaQuickmatchNoQueries = BIT10,
	HF_ViaRetry = BIT11,
	HF_ViaScriptToGame = BIT12,
	HF_JobToJob = BIT13, // to game only
	HF_Asynchronous = BIT14,
	HF_Premium = BIT15,
	HF_ViaTransitionLobby = BIT16, // to game only
	HF_JoinViaPresenceDisabled = BIT17,
	HF_JoinInProgressDisabled = BIT18,
	HF_Default = 0,
};

enum JoinFlags
{
	JF_ViaInvite = BIT0,
	JF_ConsumePrivate = BIT1,
	JF_IsAdminDev = BIT2,
	JF_IsAdmin = BIT3,
	JF_Asynchronous = BIT4,
	JF_BadReputation = BIT5,
	JF_IsBoss = BIT6,
	JF_ExpandedIntroFlow = BIT7,
	JF_Num = 8,
	JF_Default = 0,
};

enum LeaveFlags
{
	LF_ReturnToLobby = BIT0,
	LF_BlacklistSession = BIT1,
	LF_NoTransitionBail = BIT2,
	LF_Default = 0,
};

//PURPOSE
// Enumeration of possible reasons to kick a player
enum KickReason
{
	KR_Invalid = -1,
	KR_VotedOut,			// kick the player voted out
	KR_PeerComplaints,		// kick the player with the most complaints
	KR_ConnectionError,		// kicked because connection error detected
	KR_NatType,				// kick the one that has the strictest NAT
	KR_ScAdmin,				// kicked because you've been touched by the Finger of God...
	KR_ScAdminBlacklist,	// kicked because you've been touched by the Finger of God...
	KR_Num,
};

#if !__NO_OUTPUT
namespace NetworkSessionUtils
{
	const char* GetKickReasonAsString(const KickReason kickReason);
}
#endif

//PURPOSE
// Describes a setting that can be shared among players in a transition session
struct sTransitionParameter
{
	u8 m_nID; 
	u32 m_nValue;
};

// Maximum number of (active) transition parameters
static const unsigned TRANSITION_INVALID_ID = static_cast<u8>(-1);
static const unsigned TRANSITION_MAX_PARAMETERS = 55;
static const unsigned TRANSITION_MAX_STRINGS = 2;
static const unsigned TRANSITION_PARAMETER_STRING_MAX = 32; 

//PURPOSE
// Keeps track of gamers we have met while being part of a network session
class CNetworkRecentPlayers
{
public:

    //PURPOSE
    // Class constructor
    CNetworkRecentPlayers();

    //PURPOSE
    // Class destructor
    ~CNetworkRecentPlayers();

    //PURPOSE
    // Adds the specified gamer to the recent players list
    //PARAMS
    // gamerInfo - The gamer info of the gamer to add
    void AddRecentPlayer(const rlGamerInfo& gamerInfo);

	//PURPOSE
	// Checks if the specified player is in our recent players list
	//PARAMS
	// hPlayer - The handle of the player to check
	bool IsRecentPlayer(const rlGamerHandle& hPlayer) const;

    //PURPOSE
    // Clears the recent players list
    void ClearRecentPlayers();

    //PURPOSE
    // Returns the number of gamers in the gamer history, i.e.
    // gamers we've met during the current session.
    unsigned GetNumRecentPlayers() const;

    //PURPOSE
    // Returns the name of the gamer at the given index in the gamer
    // history.
    const char* GetRecentPlayerName(const unsigned idx) const;

    //PURPOSE
    // Returns the handle of the gamer at the given index in the
    // gamer history.
    const rlGamerHandle* GetRecentPlayerHandle(const unsigned idx) const;

private:

    //PURPOSE
    // Describes a gamer we have met during a multiplayer session
    struct RecentPlayer
    {
        RecentPlayer();

        void Clear();
		void Reset(const rlGamerInfo& gamerInfo);

        char m_Name[RL_MAX_NAME_LENGTH];
		rlGamerHandle m_hGamer;
		rlGamerId m_GamerId;
        inlist_node<RecentPlayer> m_ListLink;
    };

    //PURPOSE
    // Used to sort the sorted recent players array
    struct RecentPlayerPred
    {
        bool operator()(RecentPlayer* a, RecentPlayer* b) const;
    };

    typedef inlist<RecentPlayer, &RecentPlayer::m_ListLink> RecentPlayerList;

    // maximum number of gamers in the multi-player player history.
    static const unsigned MAX_RECENT_PLAYERS = 100;

    RecentPlayer m_RecentPlayersPile[MAX_RECENT_PLAYERS];		// Storage for the met gamers
    RecentPlayerList m_RecentPlayersPool;						// Linked list of free met gamers
    RecentPlayerList m_RecentPlayers;							// Linked list of met gamers in use
    RecentPlayer* m_SortedRecentPlayers[MAX_RECENT_PLAYERS];	// Gamers met in alphabetical order.
};

//PURPOSE
// Keeps track of gamers that have been blacklisted from the session
class CBlacklistedGamers
{
public:

    //PURPOSE
    // Class constructor
    CBlacklistedGamers();

    //PURPOSE
    // Class destructor
    ~CBlacklistedGamers();

    void Update(); 
    void SetBlacklistTimeout(unsigned nTimeout) { m_BlackoutTime = nTimeout; }

    //PURPOSE
    // Adds a gamer to the blacklist.  Blacklisted gamers can't join
    // sessions we host.
    //PARAMS
    // gamer - The gamer to blacklist
    //NOTES
    // There are only a limited number of blacklist slots.
    // Black list slots will be recycled in LRU order.
    // The blacklist is not cleared when the session is shut down.
    // It must be explicitly cleared.
    void BlacklistGamer(const rlGamerHandle &gamer, const eBlacklistReason nReason);

    //PURPOSE
    // Remove a gamer from the blacklist
    //PARAMS
    // gamer - The gamer to remove from the blacklist
    void WhitelistGamer(const rlGamerHandle &gamer);

    //PURPOSE
    // Removes all blacklisted gamers from the blacklist
    void ClearBlacklist();

    //PURPOSE
    // Returns true if we've kicked this gamer any time since we
    // powered on
    bool IsBlacklisted(const rlGamerHandle &gamer) const;

	//PURPOSE
	// Returns reason this gamer was blacklisted
	eBlacklistReason GetBlacklistReason(const rlGamerHandle &gamer) const;

private:

    //PURPOSE
    // Describes a blacklisted gamer
    struct BlacklistedGamer
    {
        rlGamerHandle                 m_GamerHandle; // gamer blacklisted
		eBlacklistReason			  m_Reason;		 // reason for blacklisting
        unsigned                      m_nTimeAdded;  // time added to blacklist
        inlist_node<BlacklistedGamer> m_ListLink;    // linked list node
    };

    typedef inlist<BlacklistedGamer, &BlacklistedGamer::m_ListLink> Blacklist;

    static const unsigned DEFAULT_TIMEOUT = 60 * 60 * 1000;
    static const unsigned MAX_LENOF_BLACKLIST = 16;

    BlacklistedGamer m_BlacklistPile[MAX_LENOF_BLACKLIST]; // storage for allocating blacklisted players

    Blacklist m_BlacklistPool; // Linked list of free blacklisted player structures
    Blacklist m_Blacklist;     // Linked list of currently blacklisted players

    unsigned m_BlackoutTime; 
};

//PURPOSE
// Keeps track of players we have invited into the session
class CNetworkInvitedPlayers
{
	// hold invites for 5 minutes
	static const unsigned TIME_TO_HOLD_INVITE = 60000 * 5;
	static const unsigned TIME_TO_WAIT_FOR_ACK = 20000;

public:

	enum
	{
		FLAG_VIA_PRESENCE		= BIT0,
		FLAG_ALLOW_TYPE_CHANGE	= BIT1,
		FLAG_IMPORTANT			= BIT2,
		FLAG_SCRIPT_DUMMY		= BIT3,
	};

	struct sInvitedGamer
	{
		rlGamerHandle m_hGamer;
		unsigned m_nTimeAdded;
		unsigned m_nFlags;
		unsigned m_nStatus;
		bool m_bHasAck;
		bool m_bDecisionMade; 
		bool m_bFollowUpPlatformInviteSent;
		
		sInvitedGamer()
			: m_nTimeAdded(0)
			, m_nFlags(0)
			, m_nStatus(0)
			, m_bHasAck(false)
			, m_bDecisionMade(false)
			, m_bFollowUpPlatformInviteSent(false)
		{
			m_hGamer.Clear();
		}

		sInvitedGamer(rlGamerHandle hGamer)
			: m_nTimeAdded(0)
			, m_nFlags(0)
			, m_nStatus(0)
			, m_bHasAck(false)
			, m_bDecisionMade(false)
			, m_bFollowUpPlatformInviteSent(false)
		{
			m_hGamer = hGamer;
		}

		bool operator==(const sInvitedGamer& other) const
		{
			return (other.m_hGamer == m_hGamer);
		}

		bool IsValid() const {return m_hGamer != RL_INVALID_GAMER_HANDLE;}
		bool IsViaPresence() const { return (m_nFlags & FLAG_VIA_PRESENCE) != 0; }
		bool IsImportant() const { return (m_nFlags & FLAG_IMPORTANT) != 0; }
		bool IsScriptDummy() const { return (m_nFlags & FLAG_SCRIPT_DUMMY) != 0; }
		bool IsWithinAckThreshold() const;
	};

     CNetworkInvitedPlayers(const SessionType sessionType);
    ~CNetworkInvitedPlayers();

	void Update();

	void AddInvite(const rlGamerHandle& hGamer, unsigned nFlags);
	void AddInvites(const rlGamerHandle* pGamers, const unsigned nGamers, unsigned nFlags);
	void RemoveInvite(const rlGamerHandle& hGamer);
	void RemoveInvites(const rlGamerHandle* pGamers, const unsigned nGamers);
	bool DidInvite(const rlGamerHandle& hGamer) const;
	bool DidInviteViaPresence(const rlGamerHandle& hGamer) const;
    bool AckInvite(const rlGamerHandle& hGamer);
	bool IsAcked(const rlGamerHandle& hGamer) const;
	bool SetDecisionMade(const rlGamerHandle& hGamer);
	bool IsDecisionMade(const rlGamerHandle& hGamer);

	bool SetInviteStatus(const rlGamerHandle& hGamer, unsigned nStatus);
	unsigned GetInviteStatus(const rlGamerHandle& hGamer);

	const sInvitedGamer& GetInvitedPlayer(const rlGamerHandle& hGamer) const;

    unsigned GetNumInvitedPlayers();
    const sInvitedGamer& GetInvitedPlayerFromIndex(int nIndex) const;

    void ClearAllInvites();

private:

	static const unsigned MAX_TRACKED_INVITES = 100;
	static const sInvitedGamer sm_InvalidInvitedGamer;

	SessionType m_SessionType;
    atFixedArray<sInvitedGamer, MAX_TRACKED_INVITES> m_InvitedPlayers;
};

#endif  // NETWORK_SESSION_UTILS_H
