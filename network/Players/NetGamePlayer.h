//
// NetGamePlayer.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef NETGAMEPLAYER_H
#define NETGAMEPLAYER_H

#include "rline/clan/rlclancommon.h"

#include "network/general/networkColours.h"
#include "network/Roaming/RoamingTypes.h"
#include "Network/Live/NetworkRemoteCheaterDetector.h"
#include "network/NetworkTypes.h"
#include "fwnet/netplayer.h"
#include "fwtl/pool.h"
#include "fwnet/netserialisers.h"
#include "rline/rlsessioninfo.h"
#include "vector/vector3.h"

class CPed;
class CPlayerInfo;

namespace rage {
	class grcTexture;
};

/*

    The CNonPhysicalPlayerData is used to store and serialise the data that is used to
    decide when players should switch between physical and non-physical representations
    in the network game.

    Physical players are those that have their player peds cloned and are involved in
    syncing the ambient population. Non-physical players only sync a very small state
    information to the other players that can be used to determine when to make them physical.

*/
class CNonPhysicalPlayerData : public nonPhysicalPlayerDataBase
{
public:

    FW_REGISTER_CLASS_POOL(CNonPhysicalPlayerData);

    CNonPhysicalPlayerData();
    ~CNonPhysicalPlayerData();

	CRoamingBubbleMemberInfo& GetRoamingBubbleMemberInfo();
	const CRoamingBubbleMemberInfo& GetRoamingBubbleMemberInfo() const;
	void SetRoamingBubbleMemberInfo(const CRoamingBubbleMemberInfo& info);

    const Vector3 &GetPosition() const;
    void SetPosition(const Vector3 &position);

    void Read(const datBitBuffer &messageBuffer);
    void Write(datBitBuffer &messageBuffer);
    unsigned GetSize();

    void Serialise(CSyncDataBase& serialiser)
    {
        m_RoamingBubbleMemberInfo.Serialise(serialiser);
		SERIALISE_POSITION(serialiser, m_position, "Position");
    }

    void WriteToLogFile(netLoggingInterface &log);

private:

	// membership info for the current roaming bubble the player is in
	CRoamingBubbleMemberInfo m_RoamingBubbleMemberInfo;

	// position of the player
    Vector3  m_position;        
};

class CNetGamePlayer : public netPlayer
{
	friend class CNetworkPlayerMgr;

public:
	//Maximum number of privileges per player
	static const int MAX_NUM_PRIVILEGE_TYPES = 2;

public:
	
	enum TextChatFlag
	{
		TEXT_LOCAL_HAS_KEYBOARD    = 0x0001,
		//Reflects the mute state set by the OS (e.g. the Xbox guide).
		TEXT_LOCAL_MUTED           = 0x0002,
		//Reflects the mute state set by the app.
		TEXT_LOCAL_FORCE_MUTED     = 0x0004,
		TEXT_LOCAL_BLOCKED         = 0x0008,
		TEXT_LOCAL_MASK            = 0x000F,

		TEXT_REMOTE_HAS_KEYBOARD   = 0x0010,
		TEXT_REMOTE_MUTED          = 0x0020,
		TEXT_REMOTE_FORCE_MUTED    = 0x0040,
		TEXT_REMOTE_BLOCKED        = 0x0080,
		TEXT_REMOTE_MASK           = 0x00F0,

		TEXT_LOCAL_TO_REMOTE_SHIFT = 4
	};

	enum PlayerFlags
	{
		PLAYER_FLAG_IS_BOSS			= BIT0,
		PLAYER_FLAG_NUM				= 1
	};

    CNetGamePlayer();
    ~CNetGamePlayer();

    void Shutdown();

    const rlGamerInfo& GetGamerInfo() const;

    void SetGamerInfo(const rlGamerInfo &gamerInfo);

    // Returns true if this player is the session host.
    virtual bool IsHost() const;

    //Returns true if the network player is physical (i.e. has a physical presence in the game world)
    virtual bool IsPhysical() const;

    // Returns the name of the player for logging
    const char* GetLogName() const;

    // returns the physical ped associated with this network player
    CPed* GetPlayerPed() const;

    // returns the player info associated with this network player
    CPlayerInfo* GetPlayerInfo() const;

    // sets the player info associated with this network player
    void SetPlayerInfo(CPlayerInfo *playerInfo);

	bool IsInRoamingBubble() const;
	bool IsInSameRoamingBubble(const CNetGamePlayer& player) const;

	CRoamingBubbleMemberInfo& GetRoamingBubbleMemberInfo();
	const CRoamingBubbleMemberInfo& GetRoamingBubbleMemberInfo() const;
	void SetRoamingBubbleMemberInfo(const CRoamingBubbleMemberInfo& info);
 
	bool CanAcceptMigratingObjects() const;

	const rlClanDesc& GetClanDesc() const { return m_clanMembershipInfo.m_Clan; }
	const rlClanMembershipData& GetClanMembershipInfo() const { return m_clanMembershipInfo; }
	void SetClanMembershipInfo(const rlClanMembershipData& clanMemberInfo);

	bool GetClanEmblemTXDName(char* outStr, int maxStrSize) const;
	template<int SIZE>
	bool GetClanEmblemTXDName(char (&buf)[SIZE]) const
	{
		return GetClanEmblemTXDName(buf, SIZE);
	}

	void SetCrewRankTitle(const char* crewRankTitle);
	const char* GetCrewRankTitle() const { return m_crewRankTitle; }

	bool IsRockstarDev() const { return m_bIsRockstarDev; }
	void SetIsRockstarDev(bool bIsRockstarDev) { m_bIsRockstarDev = bIsRockstarDev; }

	bool IsRockstarQA() const { return m_bIsRockstarQA; }
	void SetIsRockstarQA(bool bIsRockstarQA) { m_bIsRockstarQA = bIsRockstarQA; }

	bool IsCheater() const { return m_bIsCheater; }
	void SetIsCheater(bool bIsCheater) { m_bIsCheater = bIsCheater; }

    u16 GetCharacterRank() const { return m_nCharacterRank; }
    void SetCharacterRank(u16 nCharacterRank) { m_nCharacterRank = nCharacterRank; }

	void SetMuteData(u32 muteCount, u32 talkerCount);
	void GetMuteData(u32 &muteCount, u32& talkerCount) const { muteCount = m_MuteBeenMutedCount; talkerCount = m_MuteTalkersMetCount; }

	eMatchmakingGroup GetMatchmakingGroup() const { return m_MatchmakingGroup; }
	void SetMatchmakingGroup(eMatchmakingGroup nGroup, bool bUpdateGroup = false);

    const rlClanId& GetCrewId() const { return m_nCrewId; }
    void SetCrewId(const rlClanId& nCrewId) { m_nCrewId = nCrewId; }
    
    bool IsSpectator() const { return m_bIsSpectator; }
    void SetSpectator(bool bIsSpectator) { m_bIsSpectator = bIsSpectator; }

    bool IsInDifferentTutorialSession() const { return m_IsInDifferentTutorialSession; }
    void SetIsInDifferentTutorialSession(bool bIsInDifferentTutorialSession) { m_IsInDifferentTutorialSession = bIsInDifferentTutorialSession; }

    u8 GetGarageInstanceIndex() const { return m_GarageInstanceIndex; }
    void SetGarageInstanceIndex(u8 garageIndex) { m_GarageInstanceIndex = garageIndex; } 

	u8 GetPropertyID() const { return m_nPropertyID; }
	void SetPropertyID(u8 nPropertyID) { m_nPropertyID = nPropertyID; } 

	u8 GetMentalState() const { return m_nMentalState; }
	void SetMentalState(u8 nMentalState) { m_nMentalState = nMentalState; } 

#if RSG_PC
	u8 GetPedDensity() const { return m_fPedDensity; }
	void SetPedDensity(u8 nPedDensity) { m_fPedDensity = nPedDensity; } 
#endif

	void SetPhoneExplosiveVehicleID(ObjectId objectId) { m_VehicleWithPhoneExplosive = objectId; }
	void ClearPhoneExplosiveVehicleID() { m_VehicleWithPhoneExplosive = NETWORK_INVALID_OBJECT_ID; }
	ObjectId GetPhoneExplosiveVehicleID() const { return m_VehicleWithPhoneExplosive; }

	bool IsLocallyDisconnected() const { return m_bIsLocallyDisconnected; }
	void SetLocallyDisconnected(bool bDisconnected) { m_bIsLocallyDisconnected = bDisconnected; }

	bool HasSentTimeoutScriptEvent() const { return m_bHasSentTimeoutScriptEvent; }
	void SetSentTimeoutScriptEvent(bool bSentEvent) { m_bHasSentTimeoutScriptEvent = bSentEvent; }

	bool WasDisconnectedDuringTransition() const { return m_bWasDisconnectedDuringTransition; }
	void SetDisconnectedDuringTransition(bool bDisconnected) { m_bWasDisconnectedDuringTransition = bDisconnected; }

	bool IsLaunchingTransition() const { return m_bIsLaunchingTransition; }
	void SetLaunchingTransition(bool bIsLaunching) { m_bIsLaunchingTransition = bIsLaunching; }

	// transition session information
	void SetStartedTransition(bool bStarted);
	void SetTransitionSessionInfo(const rlSessionInfo& hInfo);
	void InvalidateTransitionSessionInfo();

	bool HasStartedTransition() const { return m_bHasStartedTransition; }
	const rlSessionInfo& GetTransitionSessionInfo() const { return m_TransitionSessionInfo; }

    // voice proximity
    void ApplyVoiceProximityOverride(Vector3 const& vOverride); 
    void ClearVoiceProximityOverride(); 
    bool HasVoiceProximityOverride() const { return m_bHasVoiceProximityOverride; }
    Vector3 const& GetVoiceProximity(Vector3& vPosition) const; 

	bool IsCheatAlreadyNotified(const u32 cheat, const u32 cheatExtraData = 0) const;
	void SetCheatAsNotified(const u32 cheat, const u32 cheatExtraData);

	// check communication Privileges
	inline void SetCommunicationPrivileges(const bool value) { m_hasCommunicationPrivileges = value; }
	inline bool GetCommunicationPrivileges() const { return m_hasCommunicationPrivileges; }

	void SetOverrideSpectatedVehicleRadio(bool bValue);
	bool IsOverrideSpectatedVehicleSet() const { return m_bOverrideSpectatedVehicleRadio; }

	//Convert local flags to remote flags.
	static unsigned LocalToRemoteTextChatFlags(const unsigned localFlags);

	//Convert remote flags to local flags.
	unsigned RemoteToLocalTextChatFlags(const unsigned remoteFlags);
	
	//Text flags we're allowed to set locally
	void SetLocalTextChatFlag(const TextChatFlag flag, const bool value);
	void SetRemoteTextChatFlags(const unsigned flags);

	bool HasKeyboard() const;

	unsigned GetLocalTextChatFlags() const;

	unsigned GetRemoteTextChatFlags() const;

	unsigned AreTextChatFlagsDirty() const;
	void CleanDirtyTextChatFlags();

#if RSG_DURANGO
	void SetDisplayName(rlDisplayName* name);
#endif

	bool IsBoss() const { return (m_PlayerFlags & PLAYER_FLAG_IS_BOSS) != 0; }
	void SetIsBoss(bool isBoss) { SetPlayerFlag(PLAYER_FLAG_IS_BOSS, isBoss); }

	//PURPOSE
	// Sets whether we should allocate a bubble for this player or not
	void SetShouldAllocateBubble(const bool bShouldAllocateBubble) { m_bShouldAllocateBubble = bShouldAllocateBubble; }
	bool GetShouldAllocateBubble() const { return m_bShouldAllocateBubble; }

	void SetSessionBailReason(const eBailReason bailReason) { m_SessionBailReason = bailReason; }
	eBailReason GetSessionBailReason() const { return m_SessionBailReason; }

	void SetPlayerAccountId(PlayerAccountId accountId) { m_PlayerAccountId = accountId; }
	PlayerAccountId GetPlayerAccountId() const { return m_PlayerAccountId; }

protected:

	//Update called (from netplayermgrbase::Update) only when this player is active
	virtual void ActiveUpdate();

	//Set the gamer data node dirty so an update will be sent.  
	virtual void DirtyGamerDataSyncNode();

	void SetPlayerFlag(unsigned nFlags, bool bValue)
	{
		if(bValue)
			m_PlayerFlags |= nFlags;
		else
			m_PlayerFlags &= ~nFlags;
	}

private:

    // The player info representing this player.
    CPlayerInfo* m_PlayerInfo;

	// Matchmaking group for this player. 
	eMatchmakingGroup m_MatchmakingGroup;
    bool m_bIsSpectator;
    rlClanId m_nCrewId;

    // This is for the script to detect this player dying
    bool m_PlayerHasJustDied;

	// membership info for the current roaming bubble the player is in
	CRoamingBubbleMemberInfo m_RoamingBubbleMemberInfo;

	//Current active Clan/crew membership info
	rlClanMembershipData m_clanMembershipInfo;
	char m_crewRankTitle[RL_CLAN_NAME_MAX_CHARS];

	// gamer data
	bool m_bIsRockstarDev;
	bool m_bIsRockstarQA;
	bool m_bIsCheater;

    // multiplayer character rank
    u16 m_nCharacterRank;

    // indicates whether the player is in a different tutorial session
    bool m_IsInDifferentTutorialSession;

	// the ID (script set) of the property the player currently owns
	u8 m_nPropertyID;

	// the mental state of the player
	u8 m_nMentalState;

	// the ped density set in the graphics settings multiplied by 10 and stored in an u8
	WIN32PC_ONLY(u8 m_fPedDensity;)

	// the index of the garage the player is currently in/viewing
    u8 m_GarageInstanceIndex;

	// object ID of vehicle rigged with phone explosive (can be invalid)
	ObjectId m_VehicleWithPhoneExplosive;  

	// track disconnected state
	bool m_bIsLocallyDisconnected;
	bool m_bHasSentTimeoutScriptEvent;
	bool m_bWasDisconnectedDuringTransition;

	// player is launching transition
	bool m_bIsLaunchingTransition;

	// transition session
	bool m_bHasStartedTransition;
	rlSessionInfo m_TransitionSessionInfo; 

    // voice proximity override
    bool m_bHasVoiceProximityOverride;
    Vector3 m_vVoiceProximityOverride;

	//Counts for number of times a player has been muted.
	u32 m_MuteBeenMutedCount;
	u32 m_MuteTalkersMetCount;

	unsigned m_TextChatFlags;
	bool m_TextChatFlagsDirty;

	// communication Privileges
	bool m_hasCommunicationPrivileges;

	// 
	bool m_bGamerDataDirtyPending; 
    
    // Do we have a reference on an emblem
	bool m_bEmblemRefd;

	//< Remembers which cheat was already notified per player (used only on cheat ids < MAX_DETECTABLE_ONCE_CHEATS). Refer to RemoteNetworkCheaterDetector.h for more info
	rage::atFixedBitSet<NetworkRemoteCheaterDetector::SIZE_OF_CHEAT_NOTIFIED_MASK, rage::u8>  m_cheatsNotified;

	static const u32 MAX_TAMPER_CODES = 20;
	rage::atFixedArray< rage::u32, MAX_TAMPER_CODES > m_cheatsTamperCodes;

	bool m_bOverrideSpectatedVehicleRadio : 1;

	unsigned m_PlayerFlags;

	// whether we should allocate a bubble for this player
	bool m_bShouldAllocateBubble;

	eBailReason m_SessionBailReason;

	PlayerAccountId m_PlayerAccountId;
};

#endif  // NETGAMEPLAYER_H
