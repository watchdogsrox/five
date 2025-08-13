//
// Networkclan.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef NETWORK_CLAN_H
#define NETWORK_CLAN_H

// Rage headers
#include "rline/clan/rlclan.h"
#include "rline/clan/rlclancommon.h"
#include "rline/ros/rlros.h"
#include "net/status.h"
#include "rline/rl.h"

// Framework headers
#include "fwnet/nettypes.h"

// Game headers
#include "network/Crews/NetworkCrewEmblemMgr.h"
#include "network/Crews/NetworkCrewMetadata.h"
#include "frontend/WarningScreen.h"
#include "Network/Live/ClanNetStatusArray.h"

namespace rage
{
	BANK_ONLY(class bkCombo;);
	BANK_ONLY(class bkToggle;);
	BANK_ONLY(class bkText;);
};

//Max number of Clan memberships
#define MAX_NUM_CLAN_MEMBERSHIPS 10

//Max number of players to retrieve clan info during a session
#define MAX_NUM_CLAN_PLAYERS MAX_NUM_PHYSICAL_PLAYERS

#define BANK_ACCOMPLISHMENT_NAME_MAX_CHARS 128
#define BANK_ACCOMPLISHMENT_COUNT_MAX_CHARS 32

//Class that represents the data needed to know 
//  for a player Clan Membership data.
class ClanMembershipData
{
	friend class NetworkClan;

private:
	rlGamerHandle            m_GamerHandle;                    // Identify Gamers
	rlClanMembershipData     m_Data[MAX_NUM_CLAN_MEMBERSHIPS]; // clan membership data
	u32                      m_Count;                          // count of memberships 
	u32                      m_TotalCount;                     // total number of memberships
	int                      m_Active;                         // -1 if the gamer doesnt have a Active clan membership
	bool                     m_Pending;                        // True when pending refresh of membership must be done

public:
	ClanMembershipData() : m_Count(0), m_TotalCount(0), m_Active(-1), m_Pending(false)
	{
	}

	//Spew clan membership data
	void  PrintMembershipData();

	//Invalidates the gamer handle,
	// the clan description and the Role id.
	bool Reset(const rlClanId& id);
	void Reset(const unsigned idx);
	void Clear();

	//Access membership
	const rlClanMembershipData*  Get(const u32 idx) const;
	const rlClanMembershipData*  GetActive() const;

	//Returns TRUE if the Clan member with index idx is valid.
	bool  IsValid(const u32 idx) const;

	//Returns TRUE if the Player clan membership is valid.
	bool  ActiveIsValid( ) const;

	//Set current active membership.
	bool  SetActiveClanMembership(const u32 index);

	//Update current active membership from scratch
	void  UpdateActiveClanMembership();

	//Add/Remove new membership data.
	bool  AddActiveClanMembershipData(const rlGamerHandle& gamer, const rlClanMembershipData& memberData);

	//Set Gamer Handle.
	void  SetGamerHandle(const rlGamerHandle& gamer);

	//Set Pending membership to be refreshed.
	void  SetPendingRefresh(const rlGamerHandle& gamer);

	//Pending membership refresh.
	bool  PendingRefresh( ) const { return m_Pending; };

	//Clear Actigve membership
	void  ClearActive() { m_Active = -1; }

	//Operator Fun
	ClanMembershipData& operator=(const ClanMembershipData& that);
};

//Responsible for managing clans.
class NetworkClan
{
public:
	//Clan Operations
	enum eNetworkClanOps
	{
		OP_NONE
		,OP_CREATE_CLAN
		,OP_LEAVE_CLAN
		,OP_KICK_CLAN_MEMBER

		,OP_INVITE_PLAYER
		,OP_REJECT_INVITE
		,OP_ACCEPT_INVITE
		,OP_CANCEL_INVITE

		,OP_ACCEPT_REQUEST
		,OP_REJECT_REQUEST

        ,OP_JOIN_OPEN_CLAN

        ,OP_RANK_CREATE
        ,OP_RANK_DELETE
        ,OP_RANK_UPDATE
        ,OP_REFRESH_RANKS

        ,OP_MEMBER_UPDATE_RANK

		,OP_REFRESH_MINE
		,OP_REFRESH_MEMBERSHIP_FOR

        ,OP_REFRESH_INVITES_RECEIVED
		,OP_REFRESH_INVITES_SENT
		,OP_REFRESH_MEMBERS
        ,OP_REFRESH_OPEN_CLANS
		,OP_REFRESH_JOIN_REQUESTS_RECEIVED
        //,OP_REFRESH_CLAN_LOOKUP
		,OP_CLAN_SEARCH
		,OP_CLAN_METADATA


		,OP_SET_PRIMARY_CLAN

        ,OP_REFRESH_WALL
        ,OP_WRITE_WALL_MESSAGE
        ,OP_DELETE_WALL_MESSAGE
		,OP_FRIEND_INFO_EVENT
	};

	//Delegate for handling callbacks from NetworkClan
	//	Usage is kind of like this.
	//
	// 	NetworkClan::Delegate netClanDlgt;
	//
	// 	void OnNetworkClanEVent(const NetworkClan::NetworkClanEvent& e)
	//	{}
	//
	// 	netClanDlgt.Bind(&OnNetworkClanEvent);
	// 	NetworkClan::AddDelegate(netClanDlgt);

	struct NetworkClanEvent
	{
		eNetworkClanOps m_opType;
		const netStatus& m_statusRef;

		NetworkClanEvent(eNetworkClanOps op, const netStatus& ref) : m_opType(op), m_statusRef(ref) {}
	};

	typedef atDelegator<void (const NetworkClanEvent&)> Delegator;
	typedef Delegator::Delegate Delegate;

	void AddDelegate(Delegate* dlgt);
	void RemoveDelegate(Delegate* dlgt);

private:

	//Delay a bit when retrieving information for remote player clans
	static const unsigned DELAY_PRIMARY_CLANS = 2000;

	//Clan Operations.
	class NetworkClanOp
	{
	public:
		NetworkClanOp() : m_pOpData(NULL) { Clear(); }
		virtual ~NetworkClanOp() { Clear(); }
		bool Pending()			const { return m_NetStatus.Pending(); }
		bool IsIdle()			const { return (netStatus::NET_STATUS_NONE == m_NetStatus.GetStatus()); }
		bool Succeeded()		const { return m_NetStatus.Succeeded(); }
		bool Failed()			const { return m_NetStatus.Failed(); }
		bool Canceled()			const { return m_NetStatus.Canceled(); }
		bool HasNoOp()					const { return OP_NONE == m_Operation; }
		bool HasBeenProcessed()			const { return (m_NeedProcessSucceed) ? m_Processed : true; }

		void Clear();
		virtual void Reset(eNetworkClanOps op);

		netStatus* GetStatus() { return &m_NetStatus; }
		const netStatus& GetStatusRef() const { return m_NetStatus; }
		eNetworkClanOps GetOp() const { return m_Operation; }

		//Simple base class for operations that need some data.
		class ClanOpData
		{
			friend class NetworkClanOp;
		protected:
			ClanOpData() {}
			virtual ~ClanOpData() {}
		};
		void SetOpData(ClanOpData* pOpData) { m_pOpData = pOpData; }
		ClanOpData* GetOpData() { return m_pOpData; }

		bool NeedProcessSucceed() { return m_NeedProcessSucceed; }
		void SetNeedProcessSucceed() { m_NeedProcessSucceed = true; }
		void SetProcessed() {	m_Processed=true;	}

	private:
		eNetworkClanOps m_Operation;
		netStatus m_NetStatus;
		ClanOpData* m_pOpData;

		bool m_NeedProcessSucceed;
		bool m_Processed;
	};

	//Special handling for multistage ops.
	class NetworkClanOp_FriendEventData : public NetworkClanOp::ClanOpData
	{
	public:
		enum FriendOpType
		{
			FOUNDED,
			JOINED
		};
		NetworkClanOp_FriendEventData(FriendOpType op, const rlGamerHandle& gh, const rlClanId clanId) 
			: NetworkClanOp::ClanOpData()
			, m_op(op)
			, m_gh(gh)
			, m_clanId(clanId)
		{
			m_displayName[0]='\0';
		}
		
		virtual ~NetworkClanOp_FriendEventData() {}

		void HandleSuccess();

		FriendOpType m_op;
		rlGamerHandle m_gh;
		rlClanId m_clanId;
		rlClanDesc m_clanDesc; //This will be the data filled in.

		rlGamertag m_gamertag;
		rlDisplayName m_displayName;
		netStatus m_gamertagRequestStatus;
	};

	// on success, will delete the invitation/request
	class NetworkClanOp_InviteRequestData : public NetworkClanOp::ClanOpData
	{
	public:
		NetworkClanOp_InviteRequestData(s64 id)
			: NetworkClanOp::ClanOpData()
			, m_Id(id)
		{

		}

		virtual ~NetworkClanOp_InviteRequestData() {}

		s64 m_Id;

		void HandleSuccess();
	};

	static const int MAX_NUM_CLAN_OPS = 10; //maximum number of concurrent operations
	NetworkClanOp m_OpPool[MAX_NUM_CLAN_OPS];

	NetworkClanOp* GetAvailableClanOp(eNetworkClanOps opType);
	bool		   CheckForAnyOpsOfType(eNetworkClanOps opType) const;

	// Script Join Clan
	CWarningMessage m_WarningMessage;
	rlClanId		m_pendingJoinClanId;

	//Hold Local Player Membership data.
	rlClanDesc           m_LocalPlayerDesc;
	ClanMembershipData   m_LocalPlayerMembership;
	u32                  m_pendingTimerSetActiveCrew;

    //Read Membership data for any player.
	ClanMembershipData   m_Membership;

    //Ranks
    static const u32 MAX_NUM_CLAN_RANKS = 10;
    rlClanRank       m_Rank[MAX_NUM_CLAN_RANKS];
	u32              m_RanksCount;
	u32              m_RanksTotalCount;

	//Open Clans
    static const u32 MAX_NUM_CLANS = 10;
    rlClanDesc       m_OpenClan[MAX_NUM_CLANS];
	u32              m_OpenClanCount;
	u32              m_OpenClanTotalCount;

	//Invites
	static const u32  MAX_NUM_CLAN_INVITES = 16; // 16 because that's how many we show on a given UI page
	rlClanInvite   m_InvitesReceived[MAX_NUM_CLAN_INVITES];
	u32            m_InvitesReceivedCount;
	u32            m_InvitesReceivedTotalCount;
	rlClanInvite   m_InvitesSent[MAX_NUM_CLAN_INVITES];
	u32            m_InvitesSentCount;
	u32            m_InvitesSentTotalCount;

	struct PendingReceivedInvite
	{
		PendingReceivedInvite()
		{
			m_ClanId = RL_INVALID_CLAN_ID;
			m_RankName[0] = '\0';
			m_Message[0] = '\0';
		}

		PendingReceivedInvite(const rlClanId clanId, const char* rankName, const char* message)
		{
			m_ClanId = clanId;
			safecpy(m_RankName, rankName);
			safecpy(m_Message, message);
		}

		// need to store this information for pending received invites
		rlClanId m_ClanId;
		char m_RankName[RL_CLAN_RANK_NAME_MAX_CHARS];
		char m_Message[RL_CLAN_INVITE_MESSAGE_MAX_CHARS];
	};

	static const u32  MAX_NUM_PENDING_RECEIVED_INVITES = 5;
	atFixedArray<PendingReceivedInvite, MAX_NUM_PENDING_RECEIVED_INVITES> m_PendingReceivedInvites;
	u32            m_PreviousInvitesReceivedCount;
	rlClanInvite   m_PreviousInvitesReceived[MAX_NUM_CLAN_INVITES];

	//Join requests
	static const u32 MAX_NUM_CLAN_JOIN_REQUESTS = 16; 
	rlClanJoinRequestRecieved m_JoinRequestsReceived[MAX_NUM_CLAN_JOIN_REQUESTS];
	u32				m_JoinRequestsReceivedCount;
	u32				m_JoinRequestsReceivedTotalCount;

	//Members
    static const u32  MAX_NUM_CLAN_MEMBERS = 10;
	rlClanMember   m_Members[MAX_NUM_CLAN_MEMBERS];
	u32            m_MembersCount;
	u32            m_MembersTotalCount;

    static const u32 MAX_NUM_CLAN_WALL_MSGS = 4;
    rlClanWallMessage m_Wall[MAX_NUM_CLAN_WALL_MSGS];
	u32               m_WallCount;
	u32               m_WallTotalCount;

	NetworkCrewEmblemMgr m_EmblemMgr;
	NetworkCrewMetadata m_primaryCrewMetadata;
	bool m_bRequestMaintenanceInviteRefresh;
	bool m_bRequestMaintenanceJoinRequestRefresh;

    //Do we have a reference on an emblem
    rlClanId m_EmblemRefd;

#if __BANK

	//Clan
	char  m_BankClanName[RL_CLAN_NAME_MAX_CHARS];
	char  m_BankClanTag[RL_CLAN_TAG_MAX_CHARS];

	//Players
	const char*  m_BankPlayers[MAX_NUM_CLAN_PLAYERS];
	bkCombo*     m_BankPlayersCombo;
	int          m_BankPlayersComboIndex;
	u32          m_BankNumPlayers;

	//Friends
	const char*  m_BankFriends[rlFriendsPage::MAX_FRIEND_PAGE_SIZE];
	bkCombo*     m_BankFriendsCombo;
	int          m_BankFriendsComboIndex;
	u32          m_BankNumFriends;

	//Memberships
	const char*  m_BankMembershipName[MAX_NUM_CLAN_MEMBERSHIPS];
	const char*  m_BankMembershipTag[MAX_NUM_CLAN_MEMBERSHIPS];
	const char*  m_BankMembershipRank[MAX_NUM_CLAN_MEMBERSHIPS];
	bkCombo*     m_BankMembershipComboName;
	bkCombo*     m_BankMembershipComboTag;
	bkCombo*     m_BankMembershipComboRank;
	int          m_BankMembershipComboIndex;
	bkCombo*     m_BankMembershipFriendsCombo;
	bkCombo*     m_BankMembershipPlayersCombo;
	u32          m_BankNumMemberships;

    //Ranks
    const char*  m_BankRanksName[MAX_NUM_CLAN_RANKS];
    u32          m_BankRankSystemFlags;
    bkCombo*     m_BankRankCombo;
    int          m_BankRankComboIndex;
    u32          m_BankNumRanks;
    char         m_BankRankUpdateName[RL_CLAN_NAME_MAX_CHARS];
    bkText*      m_BankRankUpdateNameText;
    bkToggle*    m_BankRankKick;
    bkToggle*    m_BankRankInvite;
    bkToggle*    m_BankRankPromote;
    bkToggle*    m_BankRankDemote;
    bkToggle*    m_BankRankRankManager;
    bkToggle*    m_BankRankAllowOpen;
    bkToggle*    m_BankRankWriteOnWall;
    bkToggle*    m_BankRankDeleteFromWall;

	//Open Clans
	const char*  m_BankOpenClansName[MAX_NUM_CLANS];
	const char*  m_BankOpenClansTag[MAX_NUM_CLANS];
	bkCombo*     m_BankOpenClansComboName;
	bkCombo*     m_BankOpenClansComboTag;
	int          m_BankOpenClansComboIndex;
	u32          m_BankNumOpenClans;

	int			 m_BankFindPage;
	char		 m_BankFindStr[RL_CLAN_NAME_MAX_CHARS];

	//MetaData
	static const int MAX_CREW_METADATA_ENUM = 15;
	int			 m_Bank_MetaDataclanId;
	unsigned int m_Bank_MetaDatareturnCount, m_Bank_MetaDatatotalCount;
	rlClanMetadataEnum m_aBankMetaDataEnum[MAX_CREW_METADATA_ENUM];

	//Invites
	const char*  m_BankInvitesReceived[MAX_NUM_CLAN_INVITES];
	const char*  m_BankInvitesClanReceived[MAX_NUM_CLAN_INVITES];
	bkCombo*     m_BankInvitesComboReceived;
	bkCombo*     m_BankInvitesComboClanReceived;
	int          m_BankInvitesComboIndexReceived;
	u32          m_BankNumInvitesReceived;

	const char*  m_BankInvitesSent[MAX_NUM_CLAN_INVITES];
	const char*  m_BankInvitesClanSent[MAX_NUM_CLAN_INVITES];
	bkCombo*     m_BankInvitesComboSent;
	bkCombo*     m_BankInvitesComboClanSent;
	int          m_BankInvitesComboIndexSent;
	u32          m_BankNumInvitesSent;

	//Members
	const char*  m_BankMembersName[MAX_NUM_CLAN_MEMBERS];
	const char*  m_BankMembersRank[MAX_NUM_CLAN_MEMBERS];
	bkCombo*     m_BankMembersComboName;
	bkCombo*     m_BankMembersComboRank;
	int          m_BankMembersComboIndex;
	u32          m_BankNumMembers;

	//Wall
	const char*  m_BankWallWriter[MAX_NUM_CLAN_MEMBERS];
	const char*  m_BankWallMessage[MAX_NUM_CLAN_MEMBERS];
	char         m_BankWallNewMessage[1024];
	bkCombo*     m_BankWallComboWriter;
	bkCombo*     m_BankWallComboMessage;
	int          m_BankWallComboIndex;
	u32          m_BankNumWalls;

	//Accomplishments
	char		m_BankAccomplishmentName[BANK_ACCOMPLISHMENT_NAME_MAX_CHARS];
	char		m_BankAccomplishmentCount[BANK_ACCOMPLISHMENT_COUNT_MAX_CHARS];

	//metadata
	NetworkCrewMetadata m_Bank_CrewMetadata;

	bool m_bank_UpdateClanDetailsRequested;

#endif // __BANK

public:

	NetworkClan();
	~NetworkClan();

	void  Init(const unsigned initMode);
	void  Shutdown(const u32 shutdownMode);
	void  Update();

	//Clan
	bool         ServiceIsValid() const;
	bool         CreateClan(const char* clanName, const char* clanTag, const bool isOpenClan);

    // Membership Data
	int						GetLocalGamerMembershipCount();
    u32                      GetMembershipCount(const rlGamerHandle& gamerHandle) const;
	const rlClanMembershipData*  GetMembership(const rlGamerHandle& gamerHandle, const u32 index) const;
	bool					 IsActiveMembershipValid() const;
	bool                     RefreshMemberships(const rlGamerHandle& gamerHandle, const int pageIndex);
	bool                     RefreshMembershipsPending(const rlGamerHandle& gamerHandle);
	inline bool				 RefreshMembershipsPendingAnyPlayer() const { return CheckForAnyOpsOfType(OP_REFRESH_MEMBERSHIP_FOR); }
	const rlClanMembershipData* GetLocalMembershipDataByClanId(rlClanId clanId) const;
	bool					HasMembershipsReadyOrRequested(const rlGamerHandle& gamerHandle) const { return m_Membership.m_GamerHandle.IsValid() && m_Membership.m_GamerHandle == gamerHandle; }

	//Primary Clan's
	bool                     SetPrimaryClan(const rlClanId clan);
	bool                     HasPrimaryClan();
	const rlClanDesc*        GetPrimaryClan() const { return (m_LocalPlayerDesc.IsValid() ? &m_LocalPlayerDesc : NULL); }
	bool					 LocalGamerHasMembership( rlClanId clanId) const;
	bool                     HasPrimaryClan(const rlGamerHandle& gamerHandle);
	const rlClanDesc*        GetPrimaryClan(const rlGamerHandle& gamerHandle) const;
	const rlClanMembershipData* GetPrimaryClanMembershipData(const rlGamerHandle& gamerHandle) const;
	bool                     LeaveClan();
	bool					 AttemptJoinClan(const rlClanId);
	void					 AcceptJoinPendingClan();
	void					 DeclineJoinPendingClan();
	bool                     JoinClan(const rlClanId clanId);

    // Rank
	bool               RefreshRanks();
	const rlClanRank*  GetRank(const u32 rank) const;
    u32                GetRankCount() const { return m_RanksCount; }
    bool               NextRanks();
    bool               CreateRank(const char *rankName, s64 systemFlags);
    bool               DeleteRank(const u32 rank);
    bool               UpdateRank(const u32 rank, const char *rankName, s64 systemFlags, int adjustOrder);

    // Open Clan List
    const rlClanDesc* GetOpenClan(const u32 index) const;
    u32               GetOpenClanCount() const { return m_OpenClanCount; }
    bool              RefreshOpenClans();
    bool              NextOpenClans();
    bool              JoinOpenClan(const u32 index);

	//Join Requests
	const rlClanJoinRequestRecieved* GetReceivedJoinRequest(const u32 index) const;
	const u32					     FindReceivedJoinRequestIndex(const rlClanRequestId index) const;
	u32								 GetReceivedJoinRequestCount() const { return m_JoinRequestsReceivedCount; }
	bool							 RefreshJoinRequestsReceived();
	bool							 AcceptRequest(const u32 index);
	bool							 RejectRequest(const u32 index);

	//Invites
	const rlClanInvite*  GetReceivedInvite(const u32 invite) const;
	const u32			 FindReceivedInviteIndex(const rlClanInviteId id ) const;
	const rlClanInvite*  GetSentInvite(const u32 invite) const;

	bool				 CanSendInvite(const rlGamerHandle&);
	bool				 CanReceiveInvite(const rlGamerHandle&);

	bool				 InviteGamer(const rlGamerHandle&);
	bool                 InvitePlayer(const int player);
	bool                 InviteFriend(const u32 friendIndex);
	bool                 CancelInvite(const u32 invite);
	bool                 AcceptInvite(const u32 invite);
	bool                 RejectInvite(const u32 invite);
	u32                  GetInvitesReceivedCount() const { return m_InvitesReceivedCount; }
	u32                  GetInvitesSentCount() const { return m_InvitesSentCount; }
	bool                 RefreshInvitesReceived();
	bool                 NextInvitesReceived();
	bool                 RefreshInvitesSent();
	bool                 NextInvitesSent();

	//Members

	bool				 IsMember(const rlGamerHandle& gamerHandle) const;
	const rlClanMember*  GetMember(const u32 member) const;
	bool                 KickMember(const u32 member);
	u32                  GetMemberCount() const { return m_MembersCount; }
	bool                 RefreshMembers();
	bool                 NextMembers();
    bool                 MemberUpdateRankId(const u32 member, bool promote);

	//Wall
	const rlClanWallMessage*  GetWall(const u32 msgIndex) const;
	u32                       GetWallCount() const { return m_WallCount; }
	bool                      RefreshWall();
	bool                      NextWall();
    bool                      PostToWall(const char* message);
    bool                      DeleteFromWall(const u32 msgIndex);

	//PURPOSE
	//Formats the clan tag to be displayed in the UI with the special clan tag font.
	//PARAMS
	//pClan - The clan for which we can to format the tag for
	//out_tag - the sting buffer to the work in
	//RETURN 
	//The char point that was given as out_tag
	static const int FORMATTED_CLAN_COLOR_LEN = 7; // '#rrggbb'
	static const int FORMATTED_CLAN_TAG_LEN = RL_CLAN_TAG_MAX_CHARS + 3 + FORMATTED_CLAN_COLOR_LEN; // three more for V-specific UI decorations
	static const char* GetUIFormattedClanTag(const rlClanMembershipData* pClan, char* out_tag, int size_of_buffer, bool bIgnoreRank = true);
	static const char* GetUIFormattedClanTag(const rlClanDesc &Clan, int iRankOrder, char* out_tag, int size_of_buffer );
	static const char* GetUIFormattedClanTag(bool m_IsSystemClan, bool bRockstarClan, const char* clanTag, int iRankOrder, Color32 color, char* out_tag, int size_of_buffer );

	//Operations
	static const char*          GetOperationName(const int operation);
	static bool GetIsFounderClan(const rlClanDesc &Clan);

	//Clan Emblem requests
	static NetworkCrewEmblemMgr& GetCrewEmblemMgr();
	bool RequestEmblemForClan( rlClanId clanId  ASSERT_ONLY(, const char* requesterName) );
	void ReleaseEmblemReference( rlClanId clanId  ASSERT_ONLY(, const char* releaserName) );
	const char* GetClanEmblemNameForClan( rlClanId clanId);
	bool GetClanEmblemNameForClan( rlClanId clanId, const char*& pOut_Result );
	int GetClanEmblemTxdSlotForClan( rlClanId clanId );
	bool IsEmblemForClanReady( rlClanId clanId );
	bool IsEmblemForClanReady( rlClanId clanId, const char*& pOut_Result );
	bool IsEmblemForClanFailed( rlClanId clanId );
	bool IsEmblemForClanPending( rlClanId clanId);
	bool IsEmblemForClanRetrying( rlClanId clanId ) const;

	const char* GetClanEmblemTXDNameForLocalGamerClan();

	const NetworkCrewMetadata& GetCrewMetadata() const { return m_primaryCrewMetadata; }

	static const char*  GetErrorName(const netStatus& erroredStatus);

	bool  IsPending() const { return !IsIdle(); }  //DON't USE.  TO BE REMOVE -DANC

private:

	//Clan event delegator
	rlClan::Delegate m_EventDelegator;
	void  OnClanEvent(const rlClanEvent& evt);

	rlRos::Delegate m_RosDeletator;
	// Event handler for processing Ros
	void OnRosEvent(const rlRosEvent&);

	//Our own delegator for sending events to others.
	Delegator m_Delegator;
	void DispatchEvent(const NetworkClanOp& op);

	bool  RefreshLocalMemberships(bool forceInstant = false);

	void  Clear();

	void  ProcessOps();
	void  ProcessOperationSucceeded(NetworkClanOp&);
	void  ProcessOperationFailed(NetworkClanOp&);
	void  ProcessOperationCanceled(NetworkClanOp&);

	void HandleClanCreatedSuccess();
	void HandleClanMembershipRefreshSuccess();

	bool  IsIdle()    const;

#if __BANK

public:

    void  InitWidgets();

private:

    void  Bank_Update();
	void  Bank_MakeFakeRecvdInvite();

	void  Bank_UpdateClanDetails();
	void  Bank_CreateClan();
    void  Bank_LeaveClan();
	void  Bank_SetPrimaryClan();

    void  Bank_RankComboSelect();
    void  Bank_RankCreate();
    void  Bank_RankDelete();
    void  Bank_RankUpdate();
    void  Bank_RankMoveUp();
    void  Bank_RankMoveDown();
    void  Bank_RefreshRanks();

	void  Bank_RefreshMemberships();
	void  Bank_RefreshPlayerMemberships();
    void  Bank_RefreshFriendMemberships();
	void  Bank_DumpMembershipInfo();

    void  Bank_InvitePlayerToPrimaryClan();
    void  Bank_InviteFriendToPrimaryClan();

    void  Bank_AcceptInvite();
    void  Bank_RejectInvite();
    void  Bank_CancelInvite();

    void  Bank_KickMember();
    void  Bank_PromoteMember();
    void  Bank_DemoteMember();
    void  Bank_RefreshMembers();

    void  Bank_RefreshWall();
    void  Bank_DeleteFromWall();
    void  Bank_PostToWall();

    void  Bank_JoinOpenClan();

	void Bank_SearchForFindClans();
	void Bank_RequestMetaData();
	void Bank_PrintLocalPrimaryMetadata();
	void Bank_PringDebugMetadata();

#endif // __BANK
};

typedef atSingleton<NetworkClan> NetworkClanInst;
#define NETWORK_CLAN_INST NetworkClanInst::GetInstance()

//Class used to download primary clan data for a max number of users.
class CGamersPrimaryClanData
{
public:
	CGamersPrimaryClanData() {;}
	~CGamersPrimaryClanData()
	{
		if (m_crews.m_RequestStatus.Pending())
			Cancel( );
		Clear();
	}

	bool  Start( rlGamerHandle* aGamers, const unsigned nGamers );
	void Cancel( );
	void  Clear( );

	int    GetResultSize() const {return m_crews.m_iResultSize;}
	int        GetStatus() const {return m_crews.m_RequestStatus.GetStatus();}
	bool       Succeeded() const {return m_crews.m_RequestStatus.Succeeded();}
	bool         Pending() const {return m_crews.m_RequestStatus.Pending();}
	bool            Idle() const {return (m_crews.m_RequestStatus.GetStatus() == netStatus::NET_STATUS_NONE);}

	const rlClanMember& operator[](int index) const { return m_crews[index]; }

private:
	ClanNetStatusArray< rlClanMember, RL_MAX_GAMERS_PER_SESSION > m_crews;
	atArray< rlGamerHandle > m_gamers;
};

#endif // NETWORK_CLAN_H

//eof
