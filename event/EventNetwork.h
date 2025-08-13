//
// EventNetwork.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef EVENT_NETWORK_H
#define EVENT_NETWORK_H

// --- Include Files ------------------------------------------------------------

// Framework headers
#include "fwevent/Event.h"
#include "fwnet/nettypes.h"
#include "fwtl/Pool.h"

// Rage includes
#include "script/thread.h"
#include "atl/string.h"

// Game includes
#include "event/system/EventData.h"
#include "network/NetworkTypes.h"
#include "network/NetworkInterface.h"
#include "network/Cloud/CloudManager.h"
#include "network/Players/NetGamePlayer.h"
#include "network/Objects/Entities/NetObjPhysical.h"
#include "pickups/Data/PickupIds.h"
#include "rline/rlgamerinfo.h"
#include "Peds/PlayerSpecialAbility.h"

// this is the (exact) size needed to send CEventNetworkPlayerScript
#define MAX_SCRIPTED_EVENT_DATA_SIZE (54 * sizeof(scrValue))

// an additional 4 because arrays in script have a leading int
static const unsigned SCRIPT_GAMER_HANDLE_SIZE = 13;
typedef scrValue scrGamerHandle[SCRIPT_GAMER_HANDLE_SIZE];
static const unsigned GAMER_HANDLE_SIZE = sizeof(scrGamerHandle);

static const unsigned MAX_NUM_SCRIPTS = 20;

class CGameScriptId;
class CPed;

// Structures used by network events. Duplicate structs exist in a script header. They must match as these are used to retrieve the event data.

struct sPlayerSessionData
{
	scrTextLabel63 PlayerName;
	scrValue PlayerIndex;
	scrValue PlayerTeam;
};

struct sPlayerScriptData
{
	scrTextLabel63 PlayerName;
	scrValue PlayerIndex;
	scrValue PlayerTeam;
	scrValue nSource;
	scrValue nNumScripts;
	scrValue arraySize; // size of aScripts array 
	scrValue aScripts[MAX_NUM_SCRIPTS];
	scrGamerHandle m_hPlayer;		//< Only used in EVENT_NETWORK_PLAYER_LEFT_SCRIPT. Pretty redundant in EVENT_NETWORK_PLAYER_JOIN_SCRIPT
	scrValue iPlayerFlags;
	scrValue nBailReason;
};

struct sStorePlayerData
{
    scrTextLabel31 m_szGamerTag;
    scrGamerHandle m_hGamer;
};

struct sPlayerDiedData
{
	scrTextLabel23 PlayerName;
	scrValue PlayerIndex;
	scrValue PlayerTeam;
	scrValue PlayerWantedLevel;

	scrTextLabel23 KillerName;
	scrValue KillerIndex;
	scrValue KillerTeam;
	scrValue KilledByPlayer;
	scrValue KillerWeaponType;
};

struct sEntityDamagedData
{
	scrValue VictimId;
	scrValue DamagerId;
	scrValue Damage;
	scrValue EnduranceDamage;
	scrValue VictimIncapacitated;
	scrValue VictimDestroyed;
	scrValue WeaponUsed;
	scrValue VictimSpeed;
	scrValue DamagerSpeed;
	scrValue IsResponsibleForCollision;
	scrValue IsHeadShot;
	scrValue WithMeleeWeapon;
	scrValue HitMaterial;
};

struct sPickupCollectedData
{
	scrValue PlacementReference;
	scrValue PlayerIndex;
	scrValue PickupHash;
	scrValue PickupAmount;
	scrValue PickupModel;
	scrValue PickupCollected;
};

struct sPickupRespawnData
{
	scrValue PlacementReference;
	scrValue PickupHash;
	scrValue PickupModel;
};

struct sAmbientPickupCollectedData
{
	scrValue PickupHash;
	scrValue PickupAmount;
	scrValue PlayerIndex;
	scrValue PickupModel;
	scrValue PlayerGift;
	scrValue DroppedByPed;
	scrValue PickupCollected;
	scrValue PickupIndex;
};

struct sPortablePickupCollectedData
{
	scrValue PickupID;
	scrValue PickupNetID;
	scrValue PlayerIndex;
	scrValue PickupModel;
};

struct sPlayerArrestedData
{
	scrValue ArresterIndex;
	scrValue ArresteeIndex;
	scrValue ArrestType;
};

struct sPlayerEnteredVehicleData
{
	scrValue PlayerIndex;
	scrValue VehicleIndex;
};

struct sPlayerActivatedSpecialAbilityData
{
	scrValue PlayerIndex;
	scrValue SpecialAbility;
};

struct sPlayerDeactivatedSpecialAbilityData
{
	scrValue PlayerIndex;
	scrValue SpecialAbility;
};

struct sPlayerSpecialAbilityFailedActivation
{
	scrValue PlayerIndex;
	scrValue SpecialAbility;
};

struct sInviteArrivedData
{
	scrValue InviteIndex;
	scrValue GameMode;
	scrValue IsFriend;
};

struct sInviteData
{
    scrGamerHandle m_hInviter;
	scrValue nGameMode;
	scrValue nSessionPurpose;
	scrValue bViaParty;
	scrValue nAimType;
	scrValue nActivityType;
	scrValue nActivityID;
	scrValue bIsSCTV;
    scrValue nFlags; 
	scrValue nNumBosses;
};

struct sSummonData
{
	scrValue nGameMode;
	scrValue nSessionPurpose;
};

struct sBroadcastVariablesUpdatedData
{
	scrValue ThreadId;
	scrValue Type;
};

struct sHostMigrateData
{
	scrValue ThreadId;
	scrValue HostPlayerIndex;
};

struct sJoinResponseData
{
	scrValue bWasSuccessful;
	scrValue nResponse;
};

struct sScriptEventData
{
	u8 Data[MAX_SCRIPTED_EVENT_DATA_SIZE];
};

struct sCheatEventData
{
	scrValue CheatType;
};

struct sTimedExplosionData
{
	scrValue nVehicleIndex;
	scrValue nCulpritIndex;
};

struct sDummyProjectileLaunchedData
{
	scrValue nFiringPedIndex;
	scrValue nFiredProjectileIndex;
	scrValue nWeaponType;
};

struct sVehicleProjectileLaunchedData
{
	scrValue nFiringVehicleIndex;
	scrValue nFiringPedIndex;
	scrValue nFiredProjectileIndex;
	scrValue nWeaponType;
};

struct sSignInChangedData
{
	scrValue nIndex;
	scrValue bIsActiveUser;
	scrValue bWasSignedIn;
	scrValue bWasOnline;
	scrValue bIsSignedIn;
	scrValue bIsOnline;
	scrValue bIsDuplicateSignIn;
};

struct sRosChangedData
{
	scrValue bValidCredentials;
	scrValue bValidRockstarID;
};

struct sBailData
{
	scrValue nBailReason;
	scrValue nBailErrorCode;
};

struct sLeaveSessionData
{
	scrValue nLeaveReason;
};

struct sVoiceSessionRequestData
{
	scrTextLabel31 m_szGamerTag;
	scrGamerHandle m_hGamer;
};

struct sVoiceSessionResponseData
{
	scrValue nResponse;
	scrValue nResponseCode;
	scrGamerHandle m_hGamer;
};

struct sVoiceSessionTerminatedData
{
	scrTextLabel31 m_szGamerTag;
	scrGamerHandle m_hGamer;
};

struct sCloudResponseData
{
	scrValue nRequestID;
	scrValue bWasSuccessful;
};

struct sGamePresenceEvent_StatUpdate
{
	scrValue m_statNameHash;
	scrTextLabel63 m_fromGamer;
	scrValue m_iValue;
	scrValue m_fValue;
	scrValue m_value2;
	scrTextLabel63 m_stringValue;
};

struct sPedLeftBehindData
{
	scrValue  m_PedId;
	scrValue  m_Reason;
};

struct sSessionEventData
{
	scrValue m_nEventID;
	scrValue m_nEventParam;
	scrGamerHandle m_hGamer;
};

struct sTransitionStartedData
{
	scrValue m_bSuccess;
};

struct sTransitionEventData
{
	scrValue m_nEventID;
	scrValue m_nEventParam;
    scrGamerHandle m_hGamer;
};

struct sTransitionMemberData
{
	scrTextLabel63 m_szGamerTag;
	scrGamerHandle m_hGamer;
    scrValue m_nCharacterRank;
};

struct sParameterData
{
	scrValue m_id; 
	scrValue m_value; 
};

struct sDroppedWeaponData
{
	scrValue m_ObjectGuid;
	scrValue m_PedGuid;
};

struct sTransitionParameterData
{
public:
	static const u32 MAX_NUM_PARAMS = 10;

public:
	scrGamerHandle  m_hGamer;
	scrValue        m_nNumberValues;
	scrValue        m_PADDING_SizeOfColArray; //This is necessary to match the way the memory structure as laid out in SCRIPT
	sParameterData  m_nParamData[MAX_NUM_PARAMS];
};

struct sClanEventInfo
{
	scrValue m_clanId;
	scrValue m_bPrimary;
};

struct sClanRankChangedInfo
{
	scrValue m_clanId;
	scrValue m_iRankOrder;
	scrValue m_bPromotion;
	scrTextLabel31 m_rankName;
};

struct sTransitionStringData
{
	scrGamerHandle m_hGamer;
	scrValue m_nParameterID;
	scrTextLabel31 m_szParameter;
};

struct sTransitionGamerInstructionData
{
	scrGamerHandle m_hFromGamer;
	scrGamerHandle m_hToGamer;
    scrTextLabel63 m_szGamerTag;
	scrValue m_nInstruction;
	scrValue m_nInstructionParam;
};

struct sPresenceInviteData
{
	scrTextLabel63 m_szGamerTag;
	scrGamerHandle m_hGamer;
	scrTextLabel23 m_szContentID;
	scrValue m_nPlaylistLength;
	scrValue m_nPlaylistCurrent; 
	scrValue m_bScTv;
	scrValue m_nInviteID; 
};

struct sPresenceInviteRemovedData
{
	scrValue m_nInviteID;
	scrValue m_nReason;
};

struct sInviteReplyData
{
    scrGamerHandle m_hInvitee;
    scrValue m_nStatus;
    scrValue m_nCharacterRank;
    scrTextLabel15 m_ClanTag;
    scrValue m_bDecisionMade;
    scrValue m_bDecision;
};

struct sCrewInviteReceivedData
{
	scrValue m_clanId;
	scrTextLabel31 m_clanName;
	scrTextLabel7 m_clanTag;
	scrTextLabel31 m_RankName;
	scrValue m_bHasMessage;
};

struct sCrewInviteRequestReceivedData
{
	scrValue m_clanId;
};

struct sCashTransactionLog
{
	scrValue m_TransactionId;
	scrValue m_Id;
	scrValue m_Type;
	scrValue m_Amount;
	scrValue m_ItemId;
	scrGamerHandle m_hGamer;
	scrTextLabel63 m_StrAmount;
};

struct sVehicleUndrivableData
{
	scrValue VehicleId;
	scrValue DamagerId;
	scrValue WeaponUsed;
};

struct sTextMessageData
{
    scrTextLabel63 szTextMessage;
    scrGamerHandle hGamer;
};

struct sGamePresenceEventTrigger
{
	scrValue m_nEventNameHash;
	scrValue m_nEventParam1;
	scrValue m_nEventParam2;
	scrTextLabel63 m_nEventString;
};

struct sFollowInviteData
{
    scrGamerHandle hGamer;
};

struct sAdminInviteData
{
    scrGamerHandle hGamer;
    scrValue bIsSCTV;
};

struct sSpectateLocalData
{
    scrGamerHandle hGamer;
};

struct sCloudEventData
{
    scrValue m_nEventID;
    scrValue m_nEventParam1;
    scrValue m_nEventParam2;
    scrTextLabel63 m_nEventString;
};

struct sNetworkShopTransaction
{
	scrValue m_Id;
	scrValue m_Type;
	scrValue m_ResultCode;
	scrValue m_Action;
	scrValue m_Serviceid;
	scrValue m_Bank;
	scrValue m_Wallet;
};

struct sPermissionCheckResult
{
	scrValue m_nCheckID;
	scrValue m_bAllowed;
};

struct sAppLaunchedData
{
	scrValue m_nFlags;
};

struct sSystemServiceEventData
{
	scrValue m_nEventID;
	scrValue m_nFlags;
};

struct sNetworkRequestDelayData
{
	scrValue m_delayTimeMs;
};

struct sNetworkScAdminPlayerUpdate
{
public:
	static const u32 MAX_NUM_ITEMS = 20;

public:
	scrValue m_fullRefreshRequested;
	scrValue m_updatePlayerBalance;
	scrValue m_awardTypeHash;
	scrValue m_awardAmount;
	scrValue m_padding; // Padding
	scrValue m_items[MAX_NUM_ITEMS];
};

struct sNetworkScAdminReceivedCash
{
public:
    scrValue m_characterIndex;
    scrValue m_cashAmount;
};

struct sNetworkStuntPerformed
{
public:
    scrValue m_stuntType;
    scrValue m_stuntValue;
};

struct sPlayerTimedOutData
{
	scrValue PlayerIndex;
};

struct sScMembershipData
{
	scrValue m_EventType;
	scrScMembershipInfo m_MembershipInfo;
	scrScMembershipInfo m_PrevMembershipInfo;
};

struct sScBenefitsData
{
	scrValue m_EventType;
	scrValue m_NumBenefits;
	scrValue m_BenefitValue;
};

// ================================================================================================================
// CEventNetwork
//
// Base class for all network events.
// Network game events are events generated in the game that the running script should know about and deal with.
// These can be a variety of things such as the network event to start a game, etc.
// ================================================================================================================
class CEventNetwork : public fwEvent
{
public:

	static const unsigned MAX_NETWORK_EVENTS = 500;

	virtual fwEvent* Clone() const;

	// all network events have the same priority at the moment
	virtual int GetEventPriority() const { return 0; }
	virtual atString GetName() const;

	FW_REGISTER_CLASS_POOL(CEventNetwork);

	// Only declare it virtual in the base class so it can be dead-stripped out of no-output builds.
	OUTPUT_ONLY(virtual void SpewEventInfo() const = 0);

	void SpewDeletionInfo() const;

	static bool CanCreateEvent();

protected:

	virtual fwEvent* CloneInternal() const = 0;

#if !__NO_OUTPUT
	void SpewToTTY_Impl(const char* str, ...) const;
	#define SpewToTTY(...)		CEventNetwork::SpewToTTY_Impl(__VA_ARGS__)
#else
	#define SpewToTTY(fmt,...) do {} while (0)
#endif
};

// ================================================================================================================
// CEventNetworkNoData - A handy templated class to handle network events that have no associated data
// ================================================================================================================
template<class DerivedClass, int EventType>
class CEventNetworkNoData : public CEventNetwork
{
public:

	virtual int GetEventType() const { return EventType; }

	virtual bool operator==(const fwEvent& event) const 
	{
		return (event.GetEventType() == this->GetEventType());
	}

	virtual bool RetrieveData(u8* UNUSED_PARAM(data), const unsigned UNUSED_PARAM(sizeOfData)) const 
	{
		Assertf(0, "%s: This event has no data to retrieve", GetName().c_str());
		return false;
	}

	void SpewEventInfo() const {}

protected:

	virtual fwEvent* CloneInternal() const { return rage_new DerivedClass(); }

};

// ================================================================================================================
// CEventNetworkNoData - A handy templated class to handle network events with an associated struct that is retrieved by the scripts
// ================================================================================================================
template<class DerivedClass, int EventType, class EventData>
class CEventNetworkWithData : public CEventNetwork
{
public:

	CEventNetworkWithData() {}

	CEventNetworkWithData(const EventData& eventData) : m_EventData(eventData) {}

	virtual int GetEventType() const { return EventType; }

	virtual bool operator==(const fwEvent& event) const 
	{ 
		if (event.GetEventType() == this->GetEventType())
		{
			Assert(dynamic_cast<const CEventNetworkWithData*>(&event));
			
			if (memcmp(&static_cast<const CEventNetworkWithData*>(&event)->m_EventData, &m_EventData, sizeof(m_EventData)) == 0)
			{
#if !__NO_OUTPUT
				SpewEventInfo();
				SpewToTTY("Event dumped : This event already exists on the queue\n");
#endif
				return true;
			}
		}

		return false;
	}

	virtual bool RetrieveData(u8* data, const unsigned sizeOfData) const 
	{ 
		if (AssertVerify(data) && 
			Verifyf(sizeOfData == sizeof(m_EventData), "%s: Size of struct is wrong. Size \"%u\" should be \"%" SIZETFMT "u\".", GetName().c_str(), sizeOfData, sizeof(m_EventData)))
		{
#if !__NO_OUTPUT
			SpewToTTY("Script retrieving data for event %s\n", GetName().c_str());
			SpewEventInfo();
#endif
			sysMemCpy(data, &m_EventData, sizeof(m_EventData));
			return true;
		}

		return false;
	}

protected:

	virtual fwEvent* CloneInternal() const { return rage_new DerivedClass(m_EventData); }

	mutable EventData m_EventData;
};

// ================================================================================================================
// CEventNetworkPlayerSession - base class for player session events
// ================================================================================================================
template <class DerivedClass, int Type>
class CEventNetworkPlayerSession : public CEventNetworkWithData<DerivedClass, Type, sPlayerSessionData>
{
public:

	CEventNetworkPlayerSession(const CNetGamePlayer& player) :
	CEventNetworkWithData<DerivedClass, Type, sPlayerSessionData>()
	{
		Assert(player.GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX);

		safecpy(this->m_EventData.PlayerName, player.GetGamerInfo().GetDisplayName());
		this->m_EventData.PlayerIndex.Int = player.GetPhysicalPlayerIndex();
		this->m_EventData.PlayerTeam.Int = player.GetTeam();
	}

	CEventNetworkPlayerSession(const sPlayerSessionData& eventData) : 
	CEventNetworkWithData<DerivedClass, Type, sPlayerSessionData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("... player name: \"%s\"", this->m_EventData.PlayerName);
		SpewToTTY("... player index: \"%d\"", this->m_EventData.PlayerIndex);
		SpewToTTY("... player team: \"%d\"", this->m_EventData.PlayerTeam);
	}
};

// ================================================================================================================
// CEventNetworkPlayerScript - base class for player script events
// ================================================================================================================
template<class DerivedClass, int Type>
class CEventNetworkPlayerScript : public CEventNetworkWithData<DerivedClass, Type, sPlayerScriptData>
{
public:

    enum eSource
    {
        SOURCE_NORMAL,
        SOURCE_TRANSITION,
        SOURCE_STORE,
    };

	enum FlagTypes
	{
		IS_BOSS = BIT0,
	};

	CEventNetworkPlayerScript(const scrThreadId threadId, const CNetGamePlayer& player, eSource nSource, int nBailReason) 
	{
		Assert(player.GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX);

		this->m_EventData.aScripts[0].Int = threadId;
		this->m_EventData.nNumScripts.Int = 1;
		safecpy(this->m_EventData.PlayerName, player.GetGamerInfo().GetDisplayName());
		this->m_EventData.PlayerIndex.Int = player.GetPhysicalPlayerIndex();
		this->m_EventData.PlayerTeam.Int  = player.GetTeam();
		this->m_EventData.nSource.Int     = static_cast<int>(nSource);
		this->m_EventData.nBailReason.Int = nBailReason;
		player.GetGamerInfo().GetGamerHandle().Export(this->m_EventData.m_hPlayer, sizeof(this->m_EventData.m_hPlayer));

		int playerFlags = 0;
		if(player.IsBoss())
		{
			playerFlags |= FlagTypes::IS_BOSS;
		}
		this->m_EventData.iPlayerFlags.Int = playerFlags;
	}

	CEventNetworkPlayerScript(const sPlayerScriptData& eventData) : 
	CEventNetworkWithData<DerivedClass, Type, sPlayerScriptData>(eventData)
	{
	}

	virtual bool RetrieveData(u8* data, const unsigned sizeOfData) const 
	{
		// account for script array prefix, containing the array size
		int arraySize = ((sPlayerScriptData*)data)->arraySize.Int;

		if (CEventNetworkWithData<DerivedClass, Type, sPlayerScriptData>::RetrieveData(data, sizeOfData))
		{
			((sPlayerScriptData*)data)->arraySize.Int = arraySize;
			return true;
		}

		return false;
	}

	bool CollateEvent(const netPlayer& player, eSource nSource, const scrThreadId threadId) const
	{
		if (this->m_EventData.PlayerIndex.Int == player.GetPhysicalPlayerIndex() && this->m_EventData.nSource.Int == static_cast<int>(nSource))
		{
#if !__FINAL
			SpewToTTY("%s triggered - collated with previous event", this->GetName().c_str());
			this->SpewEventInfo();
#endif // !__FINAL

			int playerFlags = 0;
			CNetGamePlayer* pPlayer = NetworkInterface::GetPhysicalPlayerFromIndex(player.GetPhysicalPlayerIndex());
			if(pPlayer && pPlayer->IsBoss())
			{
				playerFlags |= FlagTypes::IS_BOSS;
			}
			this->m_EventData.iPlayerFlags.Int = playerFlags;

			int numScripts = this->m_EventData.nNumScripts.Int;

			if (numScripts < MAX_NUM_SCRIPTS)
			{
				for (u32 i=0; i<numScripts; i++)
				{
					if (this->m_EventData.aScripts[i].Int == threadId)
					{
						return true;
					}
				}

				this->m_EventData.aScripts[numScripts].Int = threadId;
				this->m_EventData.nNumScripts.Int++;
			}

			return true;
		}

		return false;
	}

	void SpewEventInfo() const
	{
		SpewToTTY("... player name: \"%s\"", this->m_EventData.PlayerName);
		SpewToTTY("... player index: \"%d\"", this->m_EventData.PlayerIndex.Int);
		SpewToTTY("... player team: \"%d\"", this->m_EventData.PlayerTeam.Int);
		SpewToTTY("... source: %d", this->m_EventData.nSource.Int);
		SpewToTTY("... player flags: %d", this->m_EventData.iPlayerFlags.Int);

		for (u32 i=0; i<this->m_EventData.nNumScripts.Int; i++)
		{
			SpewToTTY("... script thread: \"%d\"", this->m_EventData.aScripts[i].Int);	
		}
	}
};

// ================================================================================================================
// CEventNetworkPlayerJoinSession - triggered when a player joins the game.
// ================================================================================================================
class CEventNetworkPlayerJoinSession : public CEventNetworkPlayerSession<CEventNetworkPlayerJoinSession, EVENT_NETWORK_PLAYER_JOIN_SESSION>
{
public:

	CEventNetworkPlayerJoinSession(const CNetGamePlayer& player) : 
	CEventNetworkPlayerSession<CEventNetworkPlayerJoinSession, EVENT_NETWORK_PLAYER_JOIN_SESSION>(player)
	{
	}

	CEventNetworkPlayerJoinSession(const sPlayerSessionData& eventData) : 
	CEventNetworkPlayerSession<CEventNetworkPlayerJoinSession, EVENT_NETWORK_PLAYER_JOIN_SESSION>(eventData)
	{
	}
};

// ================================================================================================================
// CEventNetworkPlayerLeftSession - triggered when a player leaves the game.
// ================================================================================================================
class CEventNetworkPlayerLeftSession : public CEventNetworkPlayerSession<CEventNetworkPlayerLeftSession, EVENT_NETWORK_PLAYER_LEFT_SESSION>
{
public:

	CEventNetworkPlayerLeftSession(const CNetGamePlayer& player) : 
	CEventNetworkPlayerSession<CEventNetworkPlayerLeftSession, EVENT_NETWORK_PLAYER_LEFT_SESSION>(player)
	{
	}

	CEventNetworkPlayerLeftSession(const sPlayerSessionData& eventData) : 
	CEventNetworkPlayerSession<CEventNetworkPlayerLeftSession, EVENT_NETWORK_PLAYER_LEFT_SESSION>(eventData)
	{
	}
};

// ================================================================================================================
// CEventNetworkPlayerJoinScript - triggered when a player joins a script session
// ================================================================================================================
class CEventNetworkPlayerJoinScript : public CEventNetworkPlayerScript<CEventNetworkPlayerJoinScript, EVENT_NETWORK_PLAYER_JOIN_SCRIPT>
{
public:

    CEventNetworkPlayerJoinScript(const scrThreadId threadId, const CNetGamePlayer& player, CEventNetworkPlayerScript::eSource nSource) :
	CEventNetworkPlayerScript<CEventNetworkPlayerJoinScript, EVENT_NETWORK_PLAYER_JOIN_SCRIPT>(threadId, player, nSource, -1)
	{
	}

	CEventNetworkPlayerJoinScript(const sPlayerScriptData& eventData) :
	CEventNetworkPlayerScript<CEventNetworkPlayerJoinScript, EVENT_NETWORK_PLAYER_JOIN_SCRIPT>(eventData)
	{
	}
};

// ================================================================================================================
// CEventNetworkPlayerLeftScript - triggered when a player leaves a script session
// ================================================================================================================
class CEventNetworkPlayerLeftScript : public CEventNetworkPlayerScript<CEventNetworkPlayerLeftScript, EVENT_NETWORK_PLAYER_LEFT_SCRIPT>
{
public:

	CEventNetworkPlayerLeftScript(const scrThreadId threadId, const CNetGamePlayer& player, CEventNetworkPlayerScript::eSource nSource, const int nBailReason) :
	CEventNetworkPlayerScript<CEventNetworkPlayerLeftScript, EVENT_NETWORK_PLAYER_LEFT_SCRIPT>(threadId, player, nSource, nBailReason)
	{
	}

	CEventNetworkPlayerLeftScript(const sPlayerScriptData& eventData) :
	CEventNetworkPlayerScript<CEventNetworkPlayerLeftScript, EVENT_NETWORK_PLAYER_LEFT_SCRIPT>(eventData)
	{
	}
};

// ================================================================================================================
// CEventNetworkStorePlayerLeft - Triggered when a player who leaves for the store times out
// ================================================================================================================
class CEventNetworkStorePlayerLeft : public CEventNetworkWithData<CEventNetworkStorePlayerLeft, EVENT_NETWORK_STORE_PLAYER_LEFT, sStorePlayerData>
{
public:

    CEventNetworkStorePlayerLeft(const rlGamerHandle& hGamer, const char* szName)
    {
        hGamer.Export(m_EventData.m_hGamer, sizeof(m_EventData.m_hGamer));
        safecpy(m_EventData.m_szGamerTag, szName);
    }

    CEventNetworkStorePlayerLeft(const sStorePlayerData& eventData) : 
    CEventNetworkWithData<CEventNetworkStorePlayerLeft, EVENT_NETWORK_STORE_PLAYER_LEFT, sStorePlayerData>(eventData)
    {
    }

    void SpewEventInfo() const
    {
        SpewToTTY("... name: %s", m_EventData.m_szGamerTag);
    }
};

// ================================================================================================================
// CEventNetworkStartGame - triggered when the network game should start.
// ================================================================================================================
class CEventNetworkStartSession : public CEventNetworkNoData<CEventNetworkStartSession, EVENT_NETWORK_SESSION_START>
{
};

// ================================================================================================================
// CEventNetworkEndSession - triggered when the network has ended and the Single Player game is re-started.
// ================================================================================================================
class CEventNetworkEndSession : public CEventNetworkWithData<CEventNetworkEndSession, EVENT_NETWORK_SESSION_END, sLeaveSessionData>
{
public:

	CEventNetworkEndSession(const LeaveSessionReason nLeaveReason)
	{
		m_EventData.nLeaveReason.Int = nLeaveReason;
	}

	CEventNetworkEndSession(const sLeaveSessionData& eventData) : 
	CEventNetworkWithData<CEventNetworkEndSession, EVENT_NETWORK_SESSION_END, sLeaveSessionData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("... Leave Reason: %d", m_EventData.nLeaveReason);
	}
};

// ================================================================================================================
// CEventNetworkStartMatch - triggered when the network game should start play.
// ================================================================================================================
class CEventNetworkStartMatch : public CEventNetworkNoData<CEventNetworkStartMatch, EVENT_NETWORK_START_MATCH>
{
};

// ================================================================================================================
// CEventNetworkEndMatch - triggered when the network game should end play.
// ================================================================================================================
class CEventNetworkEndMatch : public CEventNetworkNoData<CEventNetworkEndMatch, EVENT_NETWORK_END_MATCH>
{
};

// ================================================================================================================
// CEventNetworkRemovedFromSessionDueToStall - triggered when local player self ejects from a session due to a long stall
// ================================================================================================================
class CEventNetworkRemovedFromSessionDueToStall : public CEventNetworkNoData<CEventNetworkRemovedFromSessionDueToStall, EVENT_NETWORK_REMOVED_FROM_SESSION_DUE_TO_STALL>
{
};

// ================================================================================================================
// CEventNetworkRemovedFromSessionDueToComplaints - triggered when local player is ejected from the session by the host
// due to peer complaints
// ================================================================================================================
class CEventNetworkRemovedFromSessionDueToComplaints : public CEventNetworkNoData<CEventNetworkRemovedFromSessionDueToComplaints, EVENT_NETWORK_REMOVED_FROM_SESSION_DUE_TO_COMPLAINTS>
{

};

// ================================================================================================================
// CEventNetworkConnectionTimeout - Triggered when a remote connection times out
// ================================================================================================================
class CEventNetworkConnectionTimeout : public CEventNetworkWithData<CEventNetworkConnectionTimeout, EVENT_NETWORK_CONNECTION_TIMEOUT, sPlayerTimedOutData>
{
public:
	CEventNetworkConnectionTimeout(PhysicalPlayerIndex nPlayerIndex)
	{
		m_EventData.PlayerIndex.Int = (int)nPlayerIndex;
	}

	CEventNetworkConnectionTimeout() 
		: CEventNetworkWithData<CEventNetworkConnectionTimeout, EVENT_NETWORK_CONNECTION_TIMEOUT, sPlayerTimedOutData>()
	{
		m_EventData.PlayerIndex.Int	= INVALID_PLAYER_INDEX;
	}

	CEventNetworkConnectionTimeout(const sPlayerTimedOutData& eventData) 
		: CEventNetworkWithData<CEventNetworkConnectionTimeout, EVENT_NETWORK_CONNECTION_TIMEOUT, sPlayerTimedOutData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("...PPI: %d",m_EventData.PlayerIndex.Int);
	}
};

// ================================================================================================================
// CEventNetworkPlayerSpawn - triggered when a player respawns
// ================================================================================================================
class CEventNetworkPlayerSpawn : public CEventNetworkPlayerSession<CEventNetworkPlayerSpawn, EVENT_NETWORK_PLAYER_SPAWN>
{
public:

	CEventNetworkPlayerSpawn(const CNetGamePlayer& player) : 
	  CEventNetworkPlayerSession<CEventNetworkPlayerSpawn, EVENT_NETWORK_PLAYER_SPAWN>(player)
	  {
	  }

	  CEventNetworkPlayerSpawn(const sPlayerSessionData& eventData) : 
	  CEventNetworkPlayerSession<CEventNetworkPlayerSpawn, EVENT_NETWORK_PLAYER_SPAWN>(eventData)
	  {
	  }
};

// ================================================================================================================
// CEventNetworkEntityDamage - triggered when a entity is damaged.
// ================================================================================================================
class CEventNetworkEntityDamage : public CEventNetworkWithData<CEventNetworkEntityDamage, EVENT_NETWORK_DAMAGE_ENTITY, sEntityDamagedData>
{
public:

	CEventNetworkEntityDamage(const CNetObjPhysical* victim, const CNetObjPhysical::NetworkDamageInfo& damageInfo, const bool isResponsibleForCollision);

	CEventNetworkEntityDamage(const sEntityDamagedData& eventData) : 
	CEventNetworkWithData<CEventNetworkEntityDamage, EVENT_NETWORK_DAMAGE_ENTITY, sEntityDamagedData>(eventData)
	{
	}

	void SpewEventInfo() const;

	//Returns true if the damage event should be added.
	//  Exceptions:
	//    - 2 vehicles colliding and if one is stopped we dont want to add a damage event if the damager is the car stopped;
	static bool IsVehicleResponsibleForCollision(const CVehicle* victim, const CVehicle* damager, const u32 weaponHash);
	static CVehicle* GetVehicle(CEntity* entity);

	virtual bool operator==(const fwEvent& event) const 
	{ 
		if (CEventNetworkWithData<CEventNetworkEntityDamage, EVENT_NETWORK_DAMAGE_ENTITY, sEntityDamagedData>::operator==(event))
		{
			Assert(dynamic_cast<const CEventNetworkEntityDamage*>(&event));

			const CEventNetworkEntityDamage* pDamageEvent = static_cast<const CEventNetworkEntityDamage*>(&event);

			if (pDamageEvent->m_pVictim.Get() == m_pVictim.Get() && pDamageEvent->m_pDamager.Get() == m_pDamager.Get())
			{
				return true;
			}
		}

		return false;
	}

	virtual bool RetrieveData(u8* data, const unsigned sizeOfData) const; 

private:
	static bool  IsVehicleVictim(const CVehicle* victim, const CVehicle* damager, const bool victimDriverIsPlayer, const bool damagerDriverIsPlayer);
	static bool  PointSameDirection(const float dot);
	static bool  PointOppositeDirection(const float dot);

	void ApplyCopsCrooksModifiers(const CVehicle* victimVehicle, const CVehicle* damagerVehicle);

	virtual fwEvent* CloneInternal() const;

	RegdEnt m_pVictim;
	RegdEnt m_pDamager;
};

// ================================================================================================================
// CEventNetworkPlayerCollectedPickup - triggered when a player collects a pickup.
// ================================================================================================================
class CEventNetworkPlayerCollectedPickup : 
	public CEventNetworkWithData<CEventNetworkPlayerCollectedPickup, EVENT_NETWORK_PLAYER_COLLECTED_PICKUP, sPickupCollectedData>
{
public:

	CEventNetworkPlayerCollectedPickup(const CNetGamePlayer& player, const int placementReference, u32 pickupHash, const int pickupAmount, const u32 customModel, const int pickupCollected); 
	CEventNetworkPlayerCollectedPickup(const int placementReference, u32 pickupHash, const int pickupAmount, const u32 customModel, const int pickupCollected); 

	CEventNetworkPlayerCollectedPickup(const sPickupCollectedData& eventData) : 
	CEventNetworkWithData<CEventNetworkPlayerCollectedPickup, EVENT_NETWORK_PLAYER_COLLECTED_PICKUP, sPickupCollectedData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("... pickup ref: \"%d\"", m_EventData.PlacementReference);
		SpewToTTY("... player index: \"%d\"", m_EventData.PlayerIndex);
		SpewToTTY("... pickup hash: \"%d\"", m_EventData.PickupHash);
		SpewToTTY("... pickup amount: \"%d\"", m_EventData.PickupAmount);
		SpewToTTY("... pickup model: \"%d\"", m_EventData.PickupModel);
		SpewToTTY("... pickup amount collected: \"%d\"", m_EventData.PickupCollected);
	}
};

// ================================================================================================================
// CEventNetworkPlayerCollectedAmbientPickup - triggered when a player collects an ambient pickup.
// ================================================================================================================
class CEventNetworkPlayerCollectedAmbientPickup : 
	public CEventNetworkWithData<CEventNetworkPlayerCollectedAmbientPickup, EVENT_NETWORK_PLAYER_COLLECTED_AMBIENT_PICKUP, sAmbientPickupCollectedData>
{
public:

	CEventNetworkPlayerCollectedAmbientPickup(const CNetGamePlayer& player, u32 pickupHash, const int pickupID, const int pickupAmount, const u32 customModel, bool bPlayerGift, bool bDroppedByPed, const int pickupCollected, const int pickupIndex); 
	CEventNetworkPlayerCollectedAmbientPickup(u32 pickupHash, const int pickupID, const int pickupAmount, const u32 customModel, bool bDroppedByPed, bool bPlayerGift, const int pickupCollected, const int pickupIndex); 

	CEventNetworkPlayerCollectedAmbientPickup(const sAmbientPickupCollectedData& eventData) : 
	CEventNetworkWithData<CEventNetworkPlayerCollectedAmbientPickup, EVENT_NETWORK_PLAYER_COLLECTED_AMBIENT_PICKUP, sAmbientPickupCollectedData>(eventData)
	{
	}

	virtual bool operator==(const fwEvent& event) const
	{
		if (CEventNetworkWithData<CEventNetworkPlayerCollectedAmbientPickup, EVENT_NETWORK_PLAYER_COLLECTED_AMBIENT_PICKUP, sAmbientPickupCollectedData>::operator==(event))
		{
			Assert(dynamic_cast<const CEventNetworkPlayerCollectedAmbientPickup*>(&event));

			const CEventNetworkPlayerCollectedAmbientPickup* pEvent = static_cast<const CEventNetworkPlayerCollectedAmbientPickup*>(&event);

			if (pEvent->m_PickupID == m_PickupID)
			{
				return true;
			}
		}

		return false;
	}


	void SpewEventInfo() const
	{
		SpewToTTY("... pickup id: \"%d\"", m_PickupID);
		SpewToTTY("... pickup type: \"%d\"", m_EventData.PickupHash);
		SpewToTTY("... pickup amount: \"%d\"", m_EventData.PickupAmount);
		SpewToTTY("... player index: \"%d\"", m_EventData.PlayerIndex);
		SpewToTTY("... pickup model: \"%d\"", m_EventData.PickupModel);
		SpewToTTY("... player gift: \"%d\"", m_EventData.PlayerGift);
		SpewToTTY("... dropped by ped: \"%d\"", m_EventData.DroppedByPed);
		SpewToTTY("... player amount collected: \"%d\"", m_EventData.PickupCollected);
		SpewToTTY("... pickup index: \"%d\"", m_EventData.PickupIndex);
	}

private:

	fwEvent* CloneInternal() const 
	{ 
		CEventNetworkPlayerCollectedAmbientPickup* pClone = rage_new CEventNetworkPlayerCollectedAmbientPickup(m_EventData); 
		pClone->m_PickupID = m_PickupID;
		return pClone;
	}

	int m_PickupID;
};

// ================================================================================================================
// CEventNetworkPlayerCollectedPortablePickup - triggered when a player collects an ambient pickup.
// ================================================================================================================
class CEventNetworkPlayerCollectedPortablePickup : 
	public CEventNetworkWithData<CEventNetworkPlayerCollectedPortablePickup, EVENT_NETWORK_PLAYER_COLLECTED_PORTABLE_PICKUP, sPortablePickupCollectedData>
{
public:

	CEventNetworkPlayerCollectedPortablePickup(const CNetGamePlayer& player, const int pickupID, const ObjectId pickupNetID, const u32 customModel); 
	CEventNetworkPlayerCollectedPortablePickup(const int pickupID, const u32 customModel); 

	CEventNetworkPlayerCollectedPortablePickup(const sPortablePickupCollectedData& eventData) : 
	CEventNetworkWithData<CEventNetworkPlayerCollectedPortablePickup, EVENT_NETWORK_PLAYER_COLLECTED_PORTABLE_PICKUP, sPortablePickupCollectedData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("... pickup id: \"%d\"", m_EventData.PickupID);
		SpewToTTY("... pickup net id: \"%d\"", m_EventData.PickupNetID);
		SpewToTTY("... player id: \"%d\"", m_EventData.PlayerIndex);
		SpewToTTY("... pickup model: \"%d\"", m_EventData.PickupModel);
	}
};

// ================================================================================================================
// CEventNetworkPlayerDroppedPortablePickup - triggered when a player collects an ambient pickup.
// ================================================================================================================
class CEventNetworkPlayerDroppedPortablePickup : 
	public CEventNetworkWithData<CEventNetworkPlayerDroppedPortablePickup, EVENT_NETWORK_PLAYER_DROPPED_PORTABLE_PICKUP, sPortablePickupCollectedData>
{
public:

	CEventNetworkPlayerDroppedPortablePickup(const CNetGamePlayer& player, const int pickupID, const ObjectId pickupNetID); 
	CEventNetworkPlayerDroppedPortablePickup(const int pickupID); 

	CEventNetworkPlayerDroppedPortablePickup(const sPortablePickupCollectedData& eventData) : 
	CEventNetworkWithData<CEventNetworkPlayerDroppedPortablePickup, EVENT_NETWORK_PLAYER_DROPPED_PORTABLE_PICKUP, sPortablePickupCollectedData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("... pickup id: \"%d\"", m_EventData.PickupID);
		SpewToTTY("... player id: \"%d\"", m_EventData.PlayerIndex);
	}
};

// ================================================================================================================
// CEventNetworkPedDroppedWeapon - triggered when a ped drops a weapon
// ================================================================================================================
class CEventNetworkPedDroppedWeapon :
	public CEventNetworkWithData<CEventNetworkPedDroppedWeapon, EVENT_NETWORK_PED_DROPPED_WEAPON, sDroppedWeaponData>
{
public:
	CEventNetworkPedDroppedWeapon(int objectGuid, CPed& ped);
	CEventNetworkPedDroppedWeapon(const sDroppedWeaponData& eventData) :
	CEventNetworkWithData<CEventNetworkPedDroppedWeapon, EVENT_NETWORK_PED_DROPPED_WEAPON, sDroppedWeaponData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("... dropped weapon guid: \"%d\"", m_EventData.m_ObjectGuid);
		SpewToTTY("... ped ned Id: \"%d\"", m_PedId);
		SpewToTTY("... ped guid: \"%d\"", m_EventData.m_PedGuid);
	}

private:
	ObjectId m_PedId;
};

// ================================================================================================================
// CEventNetworkPlayerArrest - triggered when a player is arrested
// ================================================================================================================
class CEventNetworkPlayerArrest : 
	public CEventNetworkWithData<CEventNetworkPlayerArrest, EVENT_NETWORK_PLAYER_ARREST, sPlayerArrestedData>
{
public:

	CEventNetworkPlayerArrest(CPed* arrester, CPed* arrestee, int type); 

	CEventNetworkPlayerArrest(const sPlayerArrestedData& eventData) : 
	CEventNetworkWithData<CEventNetworkPlayerArrest, EVENT_NETWORK_PLAYER_ARREST, sPlayerArrestedData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("... arrester id: \"%d\"", m_EventData.ArresterIndex);
		SpewToTTY("... arrestee id: \"%d\"", m_EventData.ArresteeIndex);
		SpewToTTY("... type: \"%d\"", m_EventData.ArrestType);
	}
};

class CEventNetworkPlayerEnteredVehicle :
	public CEventNetworkWithData<CEventNetworkPlayerEnteredVehicle, EVENT_NETWORK_PLAYER_ENTERED_VEHICLE, sPlayerEnteredVehicleData>
{
public:
	CEventNetworkPlayerEnteredVehicle (const CNetGamePlayer& player, CVehicle& vehicle);
	CEventNetworkPlayerEnteredVehicle(const sPlayerEnteredVehicleData& eventData) : 
		CEventNetworkWithData<CEventNetworkPlayerEnteredVehicle, EVENT_NETWORK_PLAYER_ENTERED_VEHICLE, sPlayerEnteredVehicleData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("... player id: \"%d\"", m_EventData.PlayerIndex);
		SpewToTTY("... vehicle id: \"%d\"", m_EventData.VehicleIndex);
	}
};

// ================================================================================================================
// CEventNetworkPlayerActivatedSpecialAbility - triggered when a player activates their special ability
// ================================================================================================================
class CEventNetworkPlayerActivatedSpecialAbility :
	public CEventNetworkWithData<CEventNetworkPlayerActivatedSpecialAbility, EVENT_NETWORK_PLAYER_ACTIVATED_SPECIAL_ABILITY, sPlayerActivatedSpecialAbilityData>
{
public:
	CEventNetworkPlayerActivatedSpecialAbility(const CNetGamePlayer& player, SpecialAbilityType specialAbility);
	CEventNetworkPlayerActivatedSpecialAbility(const sPlayerActivatedSpecialAbilityData& eventData) :
		CEventNetworkWithData<CEventNetworkPlayerActivatedSpecialAbility, EVENT_NETWORK_PLAYER_ACTIVATED_SPECIAL_ABILITY, sPlayerActivatedSpecialAbilityData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("... player id: \"%d\"", m_EventData.PlayerIndex);
		SpewToTTY("... special ability id: \"%d\"", m_EventData.SpecialAbility);
	}
};

// ================================================================================================================
// CEventNetworkPlayerDeactivatedSpecialAbility - triggered when a player special ability has deactivated
// ================================================================================================================
class CEventNetworkPlayerDeactivatedSpecialAbility :
	public CEventNetworkWithData<CEventNetworkPlayerDeactivatedSpecialAbility, EVENT_NETWORK_PLAYER_DEACTIVATED_SPECIAL_ABILITY, sPlayerDeactivatedSpecialAbilityData>
{
public:
	CEventNetworkPlayerDeactivatedSpecialAbility(const CNetGamePlayer& player, SpecialAbilityType specialAbility);
	CEventNetworkPlayerDeactivatedSpecialAbility(const sPlayerDeactivatedSpecialAbilityData& eventData) :
		CEventNetworkWithData<CEventNetworkPlayerDeactivatedSpecialAbility, EVENT_NETWORK_PLAYER_DEACTIVATED_SPECIAL_ABILITY, sPlayerDeactivatedSpecialAbilityData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("... player id: \"%d\"", m_EventData.PlayerIndex);
		SpewToTTY("... special ability id: \"%d\"", m_EventData.SpecialAbility);
	}
};

// ================================================================================================================
// CEventNetworkPlayerSpecialAbilityFailedActivation - triggered when a player has attempted to use a special ability but the activation has failed
// ================================================================================================================
class CEventNetworkPlayerSpecialAbilityFailedActivation :
	public CEventNetworkWithData<CEventNetworkPlayerSpecialAbilityFailedActivation, EVENT_NETWORK_PLAYER_SPECIAL_ABILITY_FAILED_ACTIVATION, sPlayerSpecialAbilityFailedActivation>
{
public:
	CEventNetworkPlayerSpecialAbilityFailedActivation(const CNetGamePlayer& player, SpecialAbilityType specialAbility);
	CEventNetworkPlayerSpecialAbilityFailedActivation(const sPlayerSpecialAbilityFailedActivation& eventData) :
		CEventNetworkWithData<CEventNetworkPlayerSpecialAbilityFailedActivation, EVENT_NETWORK_PLAYER_SPECIAL_ABILITY_FAILED_ACTIVATION, sPlayerSpecialAbilityFailedActivation>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("... player id: \"%d\"", m_EventData.PlayerIndex);
		SpewToTTY("... special ability id: \"%d\"", m_EventData.SpecialAbility);
	}
};

// ================================================================================================================
// CEventNetworkTimedExplosion - triggered when a vehicle timed explosion is triggered
// ================================================================================================================
class CEventNetworkTimedExplosion : 
	public CEventNetworkWithData<CEventNetworkTimedExplosion, EVENT_NETWORK_TIMED_EXPLOSION, sTimedExplosionData>
{
public:

	CEventNetworkTimedExplosion(CEntity* pVehicle, CEntity* pCulprit); 
	CEventNetworkTimedExplosion(const sTimedExplosionData& eventData) : 
	CEventNetworkWithData<CEventNetworkTimedExplosion, EVENT_NETWORK_TIMED_EXPLOSION, sTimedExplosionData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("... vehicle id: \"%d\"", m_EventData.nVehicleIndex);
		SpewToTTY("... culprit id: \"%d\"", m_EventData.nCulpritIndex);
	}
};

// ================================================================================================================
// CEventNetworkFiredDummyProjectile - triggered when an NPC fires a dummy projectile that explodes close to the target without affecting it 
// ================================================================================================================
class CEventNetworkFiredDummyProjectile : 
	public CEventNetworkWithData<CEventNetworkFiredDummyProjectile, EVENT_NETWORK_FIRED_DUMMY_PROJECTILE, sDummyProjectileLaunchedData>
{
public:

	CEventNetworkFiredDummyProjectile(CEntity* pFiringPed, CEntity* pProjectile, u32 weaponType); 
	CEventNetworkFiredDummyProjectile(const sDummyProjectileLaunchedData& eventData) : 
		CEventNetworkWithData<CEventNetworkFiredDummyProjectile, EVENT_NETWORK_FIRED_DUMMY_PROJECTILE, sDummyProjectileLaunchedData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("... Firing Ped index: \"%d\"", m_EventData.nFiringPedIndex);
		SpewToTTY("... Weapon type: \"%d\"", m_EventData.nWeaponType);
	}
};

// ================================================================================================================
// CEventNetworkFiredVehicleProjectile - triggered when a projectile has been fired.
// ================================================================================================================
class CEventNetworkFiredVehicleProjectile : 
	public CEventNetworkWithData<CEventNetworkFiredVehicleProjectile, EVENT_NETWORK_FIRED_VEHICLE_PROJECTILE, sVehicleProjectileLaunchedData>
{
public:

	CEventNetworkFiredVehicleProjectile(CEntity* pFiringVehicle, CEntity* pFiringPed, CEntity* pProjectile, u32 weaponType); 
	CEventNetworkFiredVehicleProjectile(const sVehicleProjectileLaunchedData& eventData) : 
		CEventNetworkWithData<CEventNetworkFiredVehicleProjectile, EVENT_NETWORK_FIRED_VEHICLE_PROJECTILE, sVehicleProjectileLaunchedData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("... Firing Vehicle index: \"%d\"", m_EventData.nFiringVehicleIndex);
		SpewToTTY("... Firing Ped index: \"%d\"", m_EventData.nFiringPedIndex);
		SpewToTTY("... Weapon type: \"%d\"", m_EventData.nWeaponType);
	}
};

// ================================================================================================================
// CEventNetworkInviteArrived - triggered when an invite arrives.
// ================================================================================================================
class CEventNetworkInviteArrived : public CEventNetworkWithData<CEventNetworkInviteArrived, EVENT_NETWORK_INVITE_ARRIVED, sInviteArrivedData>
{
public:
	CEventNetworkInviteArrived(const unsigned index);

	CEventNetworkInviteArrived(const sInviteArrivedData& eventData) : 
	CEventNetworkWithData<CEventNetworkInviteArrived, EVENT_NETWORK_INVITE_ARRIVED, sInviteArrivedData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("... index: \"%d\"", m_EventData.InviteIndex);
		SpewToTTY("... gamemode: \"%d\"", m_EventData.GameMode);
		SpewToTTY("... isFriend: \"%s\"", (m_EventData.IsFriend.Int == TRUE) ? "true" : "false");
	}
};

// ================================================================================================================
// CEventNetworkInviteAccepted - triggered when a user accepts an invite (through Platform UI)
// ================================================================================================================
class CEventNetworkInviteAccepted : public CEventNetworkWithData<CEventNetworkInviteAccepted, EVENT_NETWORK_INVITE_ACCEPTED, sInviteData>
{
public:

	CEventNetworkInviteAccepted(const rlGamerHandle& hInviter, const unsigned gameMode, const SessionPurpose sessionPurpose, const bool bViaParty, const unsigned aimType, const int activityType, const unsigned activityId, const bool bIsSCTV, const unsigned nFlags, const u8 nNumBosses)
	{
        if(hInviter.IsValid())
		    hInviter.Export(m_EventData.m_hInviter, sizeof(m_EventData.m_hInviter));
        m_EventData.nGameMode.Int = static_cast<int>(gameMode);
		m_EventData.nSessionPurpose.Int = static_cast<int>(sessionPurpose);
		m_EventData.bViaParty.Int = bViaParty ? TRUE : FALSE;
		m_EventData.nAimType.Int = aimType;
		m_EventData.nActivityType.Int = activityType;
		m_EventData.nActivityID.Int = static_cast<int>(activityId);
		m_EventData.bIsSCTV.Int = bIsSCTV ? TRUE : FALSE;
        m_EventData.nFlags.Int = static_cast<int>(nFlags);
		m_EventData.nNumBosses.Int = static_cast<int>(nNumBosses);
	}
	
	CEventNetworkInviteAccepted(const sInviteData& eventData) : 
	CEventNetworkWithData<CEventNetworkInviteAccepted, EVENT_NETWORK_INVITE_ACCEPTED, sInviteData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("... nGameMode: \"%d\"", m_EventData.nGameMode.Int);
		SpewToTTY("... nSessionPurpose: \"%d\"", m_EventData.nSessionPurpose.Int);
		SpewToTTY("... bViaParty: \"%s\"", (m_EventData.bViaParty.Int == TRUE) ? "true" : "false");
		SpewToTTY("... nAimType: \"%d\"", m_EventData.nAimType.Int);
		SpewToTTY("... nActivityType: \"%d\"", m_EventData.nActivityType.Int);
		SpewToTTY("... nActivityID: \"%08x\"", m_EventData.nActivityID.Int);
		SpewToTTY("... bIsSCTV: \"%s\"", (m_EventData.bIsSCTV.Int == TRUE) ? "true" : "false");
		SpewToTTY("... Flags: \"%d\"", m_EventData.nFlags.Int);
		SpewToTTY("... Num Bosses: \"%d\"", m_EventData.nNumBosses.Int);
	}
};

// ================================================================================================================
// CEventNetworkInviteConfirmed - triggered when an invite arrives.
// ================================================================================================================
class CEventNetworkInviteConfirmed : public CEventNetworkWithData<CEventNetworkInviteConfirmed, EVENT_NETWORK_INVITE_CONFIRMED, sInviteData>
{
public:

	CEventNetworkInviteConfirmed(const rlGamerHandle& hInviter, const unsigned gameMode, const SessionPurpose sessionPurpose, const bool bViaParty, const unsigned aimType, const int activityType, const unsigned activityId, const bool bIsSCTV, const unsigned nFlags, const u8 nNumBosses)
	{
        if(hInviter.IsValid())
            hInviter.Export(m_EventData.m_hInviter, sizeof(m_EventData.m_hInviter));
		m_EventData.nGameMode.Int = static_cast<int>(gameMode);
		m_EventData.nSessionPurpose.Int = static_cast<int>(sessionPurpose);
		m_EventData.bViaParty.Int = bViaParty ? TRUE : FALSE;
		m_EventData.nAimType.Int = static_cast<int>(aimType);
		m_EventData.nActivityType.Int = activityType;
		m_EventData.nActivityID.Int = static_cast<int>(activityId);
		m_EventData.bIsSCTV.Int = bIsSCTV ? TRUE : FALSE;
		m_EventData.nFlags.Int = static_cast<int>(nFlags);
		m_EventData.nNumBosses.Int = static_cast<int>(nNumBosses);
	}

	CEventNetworkInviteConfirmed(const sInviteData& eventData) : 
	CEventNetworkWithData<CEventNetworkInviteConfirmed, EVENT_NETWORK_INVITE_CONFIRMED, sInviteData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("... nGameMode: \"%d\"", m_EventData.nGameMode.Int);
		SpewToTTY("... nSessionPurpose: \"%d\"", m_EventData.nSessionPurpose.Int);
		SpewToTTY("... bViaParty: \"%s\"", (m_EventData.bViaParty.Int == TRUE) ? "true" : "false");
		SpewToTTY("... nAimType: \"%d\"", m_EventData.nAimType.Int);
		SpewToTTY("... nActivityType: \"%d\"", m_EventData.nActivityType.Int);
		SpewToTTY("... nActivityID: \"%08x\"", m_EventData.nActivityID.Int);
		SpewToTTY("... bIsSCTV: \"%s\"", (m_EventData.bIsSCTV.Int == TRUE) ? "true" : "false");
		SpewToTTY("... Flags: \"%d\"", m_EventData.nFlags.Int);
		SpewToTTY("... Num Bosses: \"%d\"", m_EventData.nNumBosses.Int);
	}
};

// ================================================================================================================
// CEventNetworkInviteRejected - Triggered when a player rejects an invite
// ================================================================================================================
class CEventNetworkInviteRejected : public CEventNetworkNoData<CEventNetworkInviteRejected, EVENT_NETWORK_INVITE_REJECTED>
{
};

// ================================================================================================================
// CEventNetworkSummon - triggered when a summon is done, i.e., when the player moves into an invite session
// ================================================================================================================
class CEventNetworkSummon : public CEventNetworkWithData<CEventNetworkSummon, EVENT_NETWORK_SUMMON, sSummonData>
{
public:

	CEventNetworkSummon(const unsigned gameMode, const SessionPurpose sessionPurpose)
	{
		m_EventData.nGameMode.Int = static_cast<int>(gameMode);
		m_EventData.nSessionPurpose.Int = static_cast<int>(sessionPurpose);
	}

	CEventNetworkSummon(const sSummonData& eventData) : 
	CEventNetworkWithData<CEventNetworkSummon, EVENT_NETWORK_SUMMON, sSummonData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("... nGameMode: \"%d\"", m_EventData.nGameMode.Int);
		SpewToTTY("... nSessionPurpose: \"%d\"", m_EventData.nSessionPurpose.Int);
	}
};

// ================================================================================================================
// CEventNetworkReceivedData - Triggered when a scripted event containing data is received.
// ================================================================================================================
class CEventNetworkScriptEvent : public CEventNetworkWithData<CEventNetworkScriptEvent, EVENT_NETWORK_SCRIPT_EVENT, sScriptEventData>
{
public:

	CEventNetworkScriptEvent(void* data, const unsigned sizeOfData, const netPlayer* fromPlayer);

	CEventNetworkScriptEvent(const sScriptEventData& eventData) : 
	CEventNetworkWithData<CEventNetworkScriptEvent, EVENT_NETWORK_SCRIPT_EVENT, sScriptEventData>(eventData)
	{
	}

	// redefine this because sizeOfData is allowed to be less than the size of the EventData (script events vary in size)
	virtual bool RetrieveData(u8* data, const unsigned sizeOfData) const 
	{ 
		if (AssertVerify(data) && 
			Verifyf(sizeOfData <= sizeof(m_EventData), "%s: Size of struct (%d) is too large, it can't exceeed %" SIZETFMT "d", GetName().c_str(), sizeOfData, sizeof(m_EventData)))
		{
			sysMemCpy(data, &m_EventData, sizeOfData);
			return true;
		}

		return false;	
	}

	void SpewEventInfo() const;
};

// ================================================================================================================
// CEventNetworkPlayerSignedOffline - triggered when the local player signs out.
// ================================================================================================================
class CEventNetworkPlayerSignedOffline : public CEventNetworkNoData<CEventNetworkPlayerSignedOffline, EVENT_NETWORK_PLAYER_SIGNED_OFFLINE>
{
};

// ================================================================================================================
// CEventNetworkSignInStateChanged - Triggered when the players sign in state changes
// ================================================================================================================
class CEventNetworkSignInStateChanged : public CEventNetworkWithData<CEventNetworkSignInStateChanged, EVENT_NETWORK_SIGN_IN_STATE_CHANGED, sSignInChangedData>
{
public:

	CEventNetworkSignInStateChanged(int nIndex, bool bIsActiveUser, bool bWasSignedIn, bool bWasOnline, bool bIsSignedIn, bool bIsOnline, bool bIsDuplicateSignIn)
	{
		m_EventData.nIndex.Int = nIndex;
		m_EventData.bIsActiveUser.Int = bIsActiveUser;
		m_EventData.bWasSignedIn.Int = bWasSignedIn;
		m_EventData.bWasOnline.Int = bWasOnline;
		m_EventData.bIsSignedIn.Int = bIsSignedIn;
		m_EventData.bIsOnline.Int = bIsOnline;
		m_EventData.bIsDuplicateSignIn.Int = bIsDuplicateSignIn;
	}

	CEventNetworkSignInStateChanged(const sSignInChangedData& eventData) : 
	CEventNetworkWithData<CEventNetworkSignInStateChanged, EVENT_NETWORK_SIGN_IN_STATE_CHANGED, sSignInChangedData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("            nIndex: %d", m_EventData.nIndex.Int);
		SpewToTTY("     bIsActiveUser: %s", m_EventData.bIsActiveUser.Int ? "True" : "False");
		SpewToTTY("      bWasSignedIn: %s", m_EventData.bWasSignedIn.Int ? "True" : "False");
		SpewToTTY("        bWasOnline: %s", m_EventData.bWasOnline.Int ? "True" : "False");
		SpewToTTY("       bIsSignedIn: %s", m_EventData.bIsSignedIn.Int ? "True" : "False");
		SpewToTTY("         bIsOnline: %s", m_EventData.bIsOnline.Int ? "True" : "False");
		SpewToTTY("bIsDuplicateSignIn: %s", m_EventData.bIsDuplicateSignIn.Int ? "True" : "False");
	}
};

// ================================================================================================================
// CEventNetworkSignInChangeActioned - triggered when we actually action a sign in state change
// ================================================================================================================
class CEventNetworkSignInChangeActioned : public CEventNetworkNoData<CEventNetworkSignInChangeActioned, EVENT_NETWORK_SIGN_IN_CHANGE_ACTIONED>
{
};

// ================================================================================================================
// CEventNetworkRosChanged - Triggered when ROS settings change
// ================================================================================================================
class CEventNetworkRosChanged : public CEventNetworkWithData<CEventNetworkRosChanged, EVENT_NETWORK_NETWORK_ROS_CHANGED, sRosChangedData>
{
public:

	CEventNetworkRosChanged(bool bValidCredentials, bool bValidRockstarID)
	{
		m_EventData.bValidCredentials.Int = bValidCredentials;
		m_EventData.bValidRockstarID.Int = bValidRockstarID;
	}

	CEventNetworkRosChanged(const sRosChangedData& eventData) : 
	CEventNetworkWithData<CEventNetworkRosChanged, EVENT_NETWORK_NETWORK_ROS_CHANGED, sRosChangedData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("Valid Credentials: %s", m_EventData.bValidCredentials.Int ? "True" : "False");
		SpewToTTY("Valid Rockstar ID: %s", m_EventData.bValidRockstarID.Int ? "True" : "False");
	}
};

// ================================================================================================================
// CEventNetworkBail - triggered when the network game finishes abruptly by a sign out, connection lost, etc.
// ================================================================================================================
class CEventNetworkBail : public CEventNetworkWithData<CEventNetworkBail, EVENT_NETWORK_NETWORK_BAIL, sBailData>
{
public:

	CEventNetworkBail(int nBailReason, int nBailErrorCode)
	{
		m_EventData.nBailReason.Int = nBailReason;
		m_EventData.nBailErrorCode.Int = nBailErrorCode;
	}

	CEventNetworkBail(const sBailData& eventData) : 
	CEventNetworkWithData<CEventNetworkBail, EVENT_NETWORK_NETWORK_BAIL, sBailData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("Bail Reason: %d", m_EventData.nBailReason.Int);
		SpewToTTY("Bail Error Code: %d", m_EventData.nBailErrorCode.Int);
	}
};

// ================================================================================================================
// CEventNetworkHostMigration - triggered when the host has migrated.
// ================================================================================================================
class CEventNetworkHostMigration : 
	public CEventNetworkWithData<CEventNetworkHostMigration, EVENT_NETWORK_HOST_MIGRATION, sHostMigrateData>
{
public:

	CEventNetworkHostMigration(const scrThreadId threadId, const CNetGamePlayer& player);

	CEventNetworkHostMigration(const sHostMigrateData& eventData) : 
	CEventNetworkWithData<CEventNetworkHostMigration, EVENT_NETWORK_HOST_MIGRATION, sHostMigrateData>(eventData) 
	{
	}

	virtual bool operator==(const fwEvent& event) const 
	{ 
		if (event.GetEventType() == this->GetEventType())
		{
			Assert(dynamic_cast<const CEventNetworkHostMigration*>(&event));

			const CEventNetworkHostMigration* pMigrationEvent = static_cast<const CEventNetworkHostMigration*>(&event);

			if (pMigrationEvent->m_EventData.ThreadId == this->m_EventData.ThreadId)
			{
				// the host can migrate multiple times during a frame, if multiple players leave at once. So copy the most recent
				// host player index and dump the event.
#if !__NO_OUTPUT
				int prevId = this->m_EventData.HostPlayerIndex.Int;
#endif
				this->m_EventData.HostPlayerIndex = pMigrationEvent->m_EventData.HostPlayerIndex;
#if !__NO_OUTPUT
				SpewEventInfo();
				SpewToTTY("Event dumped : This event already exists on the queue. New host id (%d) copied to existing event (prev id: %d). \n", this->m_EventData.HostPlayerIndex, prevId);
#endif
				return true;
			}
		}

		return false;
	}

	void SpewEventInfo() const;
};

// ================================================================================================================
// CEventNetworkAttemptHostMigration - triggered when the host migration is attempted
// ================================================================================================================
class CEventNetworkAttemptHostMigration : 
	public CEventNetworkWithData<CEventNetworkAttemptHostMigration, EVENT_NETWORK_ATTEMPT_HOST_MIGRATION, sHostMigrateData>
{
public:

	CEventNetworkAttemptHostMigration(const scrThreadId threadId, const CNetGamePlayer& player);

	CEventNetworkAttemptHostMigration(const sHostMigrateData& eventData) : 
	CEventNetworkWithData<CEventNetworkAttemptHostMigration, EVENT_NETWORK_ATTEMPT_HOST_MIGRATION, sHostMigrateData>(eventData) 
	{
	}

	virtual bool operator==(const fwEvent& event) const 
	{ 
		if (event.GetEventType() == this->GetEventType())
		{
			Assert(dynamic_cast<const CEventNetworkAttemptHostMigration*>(&event));

			const CEventNetworkAttemptHostMigration* pMigrationEvent = static_cast<const CEventNetworkAttemptHostMigration*>(&event);

			if (pMigrationEvent->m_EventData.ThreadId == this->m_EventData.ThreadId)
			{
				// the host can migrate multiple times during a frame, if multiple players leave at once. So copy the most recent
				// host player index and dump the event.
#if !__NO_OUTPUT
				int prevId = this->m_EventData.HostPlayerIndex.Int;
#endif
				this->m_EventData.HostPlayerIndex = pMigrationEvent->m_EventData.HostPlayerIndex;
#if !__NO_OUTPUT
				SpewEventInfo();
				SpewToTTY("Event dumped : This event already exists on the queue. New host id (%d) copied to existing event (prev id: %d). \n", this->m_EventData.HostPlayerIndex, prevId);
#endif
				return true;
			}
		}

		return false;
	}

	void SpewEventInfo() const;
};

// ================================================================================================================
// CEventNetworkFindSession - triggered when we start the search for a session to join.
// ================================================================================================================
class CEventNetworkFindSession : public CEventNetworkNoData<CEventNetworkFindSession, EVENT_NETWORK_FIND_SESSION>
{
};

// ================================================================================================================
// CEventNetworkHostSession - triggered when a session is hosted.
// ================================================================================================================
class CEventNetworkHostSession : public CEventNetworkNoData<CEventNetworkHostSession, EVENT_NETWORK_HOST_SESSION>
{
};

// ================================================================================================================
// CEventNetworkJoinSession - triggered when we start to join a session.
// ================================================================================================================
class CEventNetworkJoinSession : public CEventNetworkNoData<CEventNetworkJoinSession, EVENT_NETWORK_JOIN_SESSION>
{
};

// ================================================================================================================
// CEventNetworkJoinSessionResponse - triggered when we have a response to a join request
// ================================================================================================================
class CEventNetworkJoinSessionResponse : public CEventNetworkWithData<CEventNetworkJoinSessionResponse, EVENT_NETWORK_JOIN_SESSION_RESPONSE, sJoinResponseData>
{

public:

	CEventNetworkJoinSessionResponse(const bool bWasSuccessful, const int nResponseCode)
	{
		m_EventData.bWasSuccessful.Int = bWasSuccessful ? TRUE : FALSE;
		m_EventData.nResponse.Int = nResponseCode;
	}

	CEventNetworkJoinSessionResponse(const sJoinResponseData& eventData) : 
	CEventNetworkWithData<CEventNetworkJoinSessionResponse, EVENT_NETWORK_JOIN_SESSION_RESPONSE, sJoinResponseData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("... Was Successful: \"%s\"", (m_EventData.bWasSuccessful.Int == TRUE) ? "true" : "false");
		SpewToTTY("... Request Code: \"%d\"", m_EventData.nResponse.Int);
	}
};

// ================================================================================================================
// CEventNetworkCheatTriggered - triggered when a player uses a cheat (give weapons, invincible, etc)
// ================================================================================================================
class CEventNetworkCheatTriggered : public CEventNetworkWithData<CEventNetworkCheatTriggered, EVENT_NETWORK_CHEAT_TRIGGERED, sCheatEventData>
{
public:

	enum eCheatType
	{
		CHEAT_INVINCIBLE,
		CHEAT_WEAPONS,
		CHEAT_HEALTH,
		CHEAT_VEHICLE,
		CHEAT_WARP
	};

public:

	CEventNetworkCheatTriggered(const eCheatType cheatType)
	{
		m_EventData.CheatType.Int = cheatType;
	}

	CEventNetworkCheatTriggered(const sCheatEventData& eventData) : 
	CEventNetworkWithData<CEventNetworkCheatTriggered, EVENT_NETWORK_CHEAT_TRIGGERED, sCheatEventData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		switch (m_EventData.CheatType.Int)
		{
		case CHEAT_INVINCIBLE:
			SpewToTTY("... cheat: INVINCIBLE");
			break;
		case CHEAT_WEAPONS:
			SpewToTTY("... cheat: WEAPONS");
			break;
		case CHEAT_HEALTH:
			SpewToTTY("... cheat: HEALTH");
			break;
		case CHEAT_VEHICLE:
			SpewToTTY("... cheat: VEHICLE");
			break;
		case CHEAT_WARP:
			SpewToTTY("... cheat: WARP");
			break;
		default:
			Assertf(0, "CEventNetworkCheatTriggered: unrecognised cheat type");
		}
	}
};

// ================================================================================================================
// CEventNetworkIncrementStat - Trigged when a stat is incremented over the network (eg, player got reported)
// ================================================================================================================
class CEventNetworkIncrementStat : public CEventNetworkNoData<CEventNetworkIncrementStat, EVENT_NETWORK_INCREMENT_STAT>
{
};

// ================================================================================================================
// CEventNetworkPrimaryClanChanged - triggered when the primary clan membership changes.
// ================================================================================================================
class CEventNetworkPrimaryClanChanged : public CEventNetworkNoData<CEventNetworkPrimaryClanChanged, EVENT_NETWORK_PRIMARY_CLAN_CHANGED>
{
};

// ================================================================================================================
// CEventNetworkClanJoined - triggered when we join a clan.
// ================================================================================================================
class CEventNetworkClanJoined : public CEventNetworkNoData<CEventNetworkClanJoined, EVENT_NETWORK_CLAN_JOINED>
{
};

// ================================================================================================================
// CEventNetworkClanLeft - triggered when we leave a clan.
// ================================================================================================================
class CEventNetworkClanLeft : public CEventNetworkWithData<CEventNetworkClanLeft, EVENT_NETWORK_CLAN_LEFT, sClanEventInfo>
{
public:
	CEventNetworkClanLeft(rlClanId clanId, bool bPrimary)
	{
		m_EventData.m_clanId.Int = (u32)clanId;
		m_EventData.m_bPrimary.Int = bPrimary;
	}

	CEventNetworkClanLeft() :
	CEventNetworkWithData<CEventNetworkClanLeft, EVENT_NETWORK_CLAN_LEFT, sClanEventInfo>()
	{
		m_EventData.m_clanId.Int = (u32)RL_INVALID_CLAN_ID;
		m_EventData.m_bPrimary.Int = false;
	}

	CEventNetworkClanLeft(const sClanEventInfo& eventData) :
	CEventNetworkWithData<CEventNetworkClanLeft, EVENT_NETWORK_CLAN_LEFT, sClanEventInfo>(eventData)
	{

	}

	void SpewEventInfo() const
	{
		SpewToTTY("... crewID: %d", m_EventData.m_clanId.Int);
		SpewToTTY("... Primary: %s", m_EventData.m_bPrimary.Int ? "TRUE" : "FALSE");
	}
};

// ================================================================================================================
// CEventNetworkClanInviteReceived - triggered when we receive a clan invite.
// ================================================================================================================
class CEventNetworkClanInviteReceived : public CEventNetworkWithData<CEventNetworkClanInviteReceived, EVENT_NETWORK_CLAN_INVITE_RECEIVED, sCrewInviteReceivedData>
{
public:
	CEventNetworkClanInviteReceived(rlClanId clanId, const char* clanName, const char* clanTag, const char* rankName, bool bMessage)
	{
		m_EventData.m_clanId.Int = (u32)clanId;
		safecpy(m_EventData.m_clanName, clanName);
		safecpy(m_EventData.m_clanTag, clanTag);
		safecpy(m_EventData.m_RankName, rankName);
		m_EventData.m_bHasMessage.Int = bMessage ? 1 : 0;
	}

	CEventNetworkClanInviteReceived() 
		: CEventNetworkWithData<CEventNetworkClanInviteReceived, EVENT_NETWORK_CLAN_INVITE_RECEIVED, sCrewInviteReceivedData>()
	{
		m_EventData.m_clanId.Int		= (u32)RL_INVALID_CLAN_ID;
		m_EventData.m_clanName[0]		= '\0';
		m_EventData.m_clanTag[0]		= '\0'; 
		m_EventData.m_RankName[0]		= '\0';
		m_EventData.m_bHasMessage.Int	= 0;
	}

	CEventNetworkClanInviteReceived(const sCrewInviteReceivedData& eventData) 
		: CEventNetworkWithData<CEventNetworkClanInviteReceived, EVENT_NETWORK_CLAN_INVITE_RECEIVED, sCrewInviteReceivedData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("...crewid: %d",m_EventData.m_clanId.Int);
		SpewToTTY("...crew name: '%s'",m_EventData.m_clanName);
		SpewToTTY("...crew tag: '%s'",m_EventData.m_clanTag);
		SpewToTTY("...rank:	'%s'",m_EventData.m_RankName);
		SpewToTTY("...Has Message: %s",m_EventData.m_bHasMessage.Int == 0 ? "false" : "true");
	}
};

// ================================================================================================================
// CEventNetworkClanInviteRequestReceived - triggered when we receive a clan invite request.
// ================================================================================================================
class CEventNetworkClanInviteRequestReceived : public CEventNetworkWithData<CEventNetworkClanInviteRequestReceived, EVENT_NETWORK_CLAN_INVITE_REQUEST_RECEIVED, sCrewInviteRequestReceivedData>
{
public:
	CEventNetworkClanInviteRequestReceived(rlClanId clanId)
	{
		m_EventData.m_clanId.Int = (u32)clanId;
	}

	CEventNetworkClanInviteRequestReceived() 
		: CEventNetworkWithData<CEventNetworkClanInviteRequestReceived, EVENT_NETWORK_CLAN_INVITE_REQUEST_RECEIVED, sCrewInviteRequestReceivedData>()
	{
		m_EventData.m_clanId.Int		= (u32)RL_INVALID_CLAN_ID;
	}

	CEventNetworkClanInviteRequestReceived(const sCrewInviteRequestReceivedData& eventData) 
		: CEventNetworkWithData<CEventNetworkClanInviteRequestReceived, EVENT_NETWORK_CLAN_INVITE_REQUEST_RECEIVED, sCrewInviteRequestReceivedData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("...crewid: %d",m_EventData.m_clanId.Int);
	}
};


// ================================================================================================================
// CEventNetworkClanKicked - triggered when we're kicked from a clan
// ================================================================================================================
class CEventNetworkClanKicked : public CEventNetworkWithData<CEventNetworkClanKicked, EVENT_NETWORK_CLAN_KICKED, sClanEventInfo>
{
public:
	CEventNetworkClanKicked(rlClanId clanId, bool bPrimary)
	{
		m_EventData.m_clanId.Int = (u32)clanId;
		m_EventData.m_bPrimary.Int = bPrimary;
	}

	CEventNetworkClanKicked(const sClanEventInfo& eventData) :
	CEventNetworkWithData<CEventNetworkClanKicked, EVENT_NETWORK_CLAN_KICKED, sClanEventInfo>(eventData)
	{

	}

	void SpewEventInfo() const
	{
		SpewToTTY("... crewID: %d", m_EventData.m_clanId.Int);
		SpewToTTY("... Primary: %s", m_EventData.m_bPrimary.Int ? "TRUE" : "FALSE");
	}
};

class CEventNetworkClanRankChanged : public CEventNetworkWithData<CEventNetworkClanRankChanged, EVENT_NETWORK_CLAN_RANK_CHANGE, sClanRankChangedInfo>
{
public:
	CEventNetworkClanRankChanged(rlClanId clanId, int rankOrder, const char* rankName, bool bPromotion)
	{
		m_EventData.m_clanId.Int = (int)clanId;
		m_EventData.m_iRankOrder.Int = rankOrder;
		safecpy(m_EventData.m_rankName, rankName);
		m_EventData.m_bPromotion.Int = bPromotion ? 1 : 0;
	}

	CEventNetworkClanRankChanged(const sClanRankChangedInfo& eventData) :
		CEventNetworkWithData<CEventNetworkClanRankChanged, EVENT_NETWORK_CLAN_RANK_CHANGE, sClanRankChangedInfo>(eventData)
		{

		}

	void SpewEventInfo() const
	{
		SpewToTTY("... crewID: %d", m_EventData.m_clanId.Int);
		SpewToTTY("... iRankOrder: %d", m_EventData.m_iRankOrder.Int);
		SpewToTTY("... rankName: %s", m_EventData.m_rankName);
		SpewToTTY("... promotion: %s", m_EventData.m_bPromotion.Int != 0 ? "true" : "false" );
	}
};

// ================================================================================================================
// CEventNetworkVoiceSessionStarted - triggered when we successfully start a voice session (host / client)
// ================================================================================================================
class CEventNetworkVoiceSessionStarted : public CEventNetworkNoData<CEventNetworkVoiceSessionStarted, EVENT_VOICE_SESSION_STARTED>
{
};

// ================================================================================================================
// CEventNetworkVoiceSessionEnded - triggered when we leave a voice session
// ================================================================================================================
class CEventNetworkVoiceSessionEnded : public CEventNetworkNoData<CEventNetworkVoiceSessionEnded, EVENT_VOICE_SESSION_ENDED>
{
};

// ================================================================================================================
// CEventNetworkVoiceConnectionRequested - triggered when a connection is requested by remote player
// ================================================================================================================
class CEventNetworkVoiceConnectionRequested : public CEventNetworkWithData<CEventNetworkVoiceConnectionRequested, EVENT_VOICE_CONNECTION_REQUESTED, sVoiceSessionRequestData>
{
public:

	CEventNetworkVoiceConnectionRequested(const rlGamerHandle& hGamer, const char* szName)
	{
		hGamer.Export(m_EventData.m_hGamer, sizeof(m_EventData.m_hGamer));
		safecpy(m_EventData.m_szGamerTag, szName);
	}

	CEventNetworkVoiceConnectionRequested(const sVoiceSessionRequestData& eventData) : 
	CEventNetworkWithData<CEventNetworkVoiceConnectionRequested, EVENT_VOICE_CONNECTION_REQUESTED, sVoiceSessionRequestData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("... name: %s", m_EventData.m_szGamerTag);
	}
};

// ================================================================================================================
// CEventNetworkVoiceConnectionResponse - triggered when we receive a response to a voice connection request
// ================================================================================================================
class CEventNetworkVoiceConnectionResponse : public CEventNetworkWithData<CEventNetworkVoiceConnectionResponse, EVENT_VOICE_CONNECTION_RESPONSE, sVoiceSessionResponseData>
{
public:

	enum eResponse
	{
		RESPONSE_ACCEPTED,
		RESPONSE_REJECTED,
		RESPONSE_BUSY,
		RESPONSE_TIMED_OUT,
		RESPONSE_ERROR,
		RESPONSE_BLOCKED,
	};

public:

	CEventNetworkVoiceConnectionResponse(const rlGamerHandle& hGamer, const eResponse nResponse, const int nResponseCode)
	{
		hGamer.Export(m_EventData.m_hGamer, sizeof(m_EventData.m_hGamer));
		m_EventData.nResponse.Int = nResponse;
		m_EventData.nResponseCode.Int = nResponseCode;
	}

	CEventNetworkVoiceConnectionResponse(const sVoiceSessionResponseData& eventData) : 
	CEventNetworkWithData<CEventNetworkVoiceConnectionResponse, EVENT_VOICE_CONNECTION_RESPONSE, sVoiceSessionResponseData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		switch (m_EventData.nResponse.Int)
		{
		case RESPONSE_ACCEPTED:
			SpewToTTY("... response: ACCEPTED");
			break;
		case RESPONSE_REJECTED:
			SpewToTTY("... response: REJECTED");
			break;
		case RESPONSE_BUSY:
			SpewToTTY("... response: BUSY");
			break;
		case RESPONSE_TIMED_OUT:
			SpewToTTY("... response: TIMED OUT");
			break;
		case RESPONSE_ERROR:
			SpewToTTY("... response: ERROR");
			break;
		default:
			Assertf(0, "CEventNetworkVoiceConnectionResponse: unrecognised response");
		}
		SpewToTTY("... Response Code: %d", m_EventData.nResponseCode.Int);
	}
};

// ================================================================================================================
// CEventNetworkVoiceConnectionTerminated - triggered when a connection is terminated (player leaves)
// ================================================================================================================
class CEventNetworkVoiceConnectionTerminated : public CEventNetworkWithData<CEventNetworkVoiceConnectionTerminated, EVENT_VOICE_CONNECTION_TERMINATED, sVoiceSessionTerminatedData>
{
public:

	CEventNetworkVoiceConnectionTerminated(const rlGamerHandle& hGamer, const char* szName)
	{
		hGamer.Export(m_EventData.m_hGamer, sizeof(m_EventData.m_hGamer));
		safecpy(m_EventData.m_szGamerTag, szName);
	}

	CEventNetworkVoiceConnectionTerminated(const sVoiceSessionTerminatedData& eventData) : 
	CEventNetworkWithData<CEventNetworkVoiceConnectionTerminated, EVENT_VOICE_CONNECTION_TERMINATED, sVoiceSessionTerminatedData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("... name: %s", m_EventData.m_szGamerTag);
	}
};

// ================================================================================================================
// CEventNetworkTextMessageReceived - 
// ================================================================================================================
class CEventNetworkTextMessageReceived : public CEventNetworkWithData<CEventNetworkTextMessageReceived, EVENT_TEXT_MESSAGE_RECEIVED, sTextMessageData>
{
public:

    CEventNetworkTextMessageReceived(const char* szText, const rlGamerHandle& hGamer)
    {
        if(hGamer.IsValid())
            hGamer.Export(m_EventData.hGamer, sizeof(m_EventData.hGamer));
        safecpy(m_EventData.szTextMessage, szText);
    }

    CEventNetworkTextMessageReceived(const sTextMessageData& eventData) : 
    CEventNetworkWithData<CEventNetworkTextMessageReceived, EVENT_TEXT_MESSAGE_RECEIVED, sTextMessageData>(eventData)
    {
    }

    void SpewEventInfo() const
    {
        SpewToTTY("... Text Message : %s", m_EventData.szTextMessage);
    }
};

// ================================================================================================================
// CEventNetworkCloudFileResponse - triggered when a cloud file request has finished
// ================================================================================================================
class CEventNetworkCloudFileResponse : public CEventNetworkWithData<CEventNetworkCloudFileResponse, EVENT_CLOUD_FILE_RESPONSE, sCloudResponseData>
{

public:

	CEventNetworkCloudFileResponse(const CloudRequestID nRequestID, bool bWasSuccessful)
	{
		m_EventData.nRequestID.Int = nRequestID;
		m_EventData.bWasSuccessful.Int = bWasSuccessful ? TRUE : FALSE;
	}

	CEventNetworkCloudFileResponse(const sCloudResponseData& eventData) : 
	CEventNetworkWithData<CEventNetworkCloudFileResponse, EVENT_CLOUD_FILE_RESPONSE, sCloudResponseData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("... RequestID: \"%d\"", m_EventData.nRequestID.Int);
		SpewToTTY("... Was Successful: \"%s\"", (m_EventData.bWasSuccessful.Int == TRUE) ? "true" : "false");
	}
};

// ================================================================================================================
// CEventNetworkPickupRespawned - triggered when a pickup placement respawns a pickup.
// ================================================================================================================
class CEventNetworkPickupRespawned : 
	public CEventNetworkWithData<CEventNetworkPickupRespawned, EVENT_NETWORK_PICKUP_RESPAWNED, sPickupRespawnData>
{
public:

	CEventNetworkPickupRespawned(const int placementReference, u32 pickupHash, const u32 customModel)
	{
		m_EventData.PlacementReference.Int	= placementReference;
		m_EventData.PickupHash.Int			= pickupHash;
		m_EventData.PickupModel.Int			= customModel;
	}

	CEventNetworkPickupRespawned(const sPickupRespawnData& eventData) : 
	CEventNetworkWithData<CEventNetworkPickupRespawned, EVENT_NETWORK_PICKUP_RESPAWNED, sPickupRespawnData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("... pickup ref: \"%d\"", m_EventData.PlacementReference.Int);
		SpewToTTY("... pickup hash: \"%d\"", m_EventData.PickupHash.Int);
		SpewToTTY("... pickup model: \"%d\"", m_EventData.PickupModel.Int);
	}
};

// ================================================================================================================
// CEventNetworkPresence_StatUpdate - triggered when a presence message comes in related to a stat update
// ================================================================================================================
class CEventNetworkPresence_StatUpdate : 
	public CEventNetworkWithData<CEventNetworkPresence_StatUpdate, EVENT_NETWORK_PRESENCE_STAT_UPDATE, sGamePresenceEvent_StatUpdate>
{
public:
	CEventNetworkPresence_StatUpdate(const u32 statHash, const char* fromGamer, int value, int value2, const char* stringData)
	{
		m_EventData.m_statNameHash.Int = statHash;
		safecpy(m_EventData.m_fromGamer, fromGamer);
		m_EventData.m_iValue.Int = value;
		m_EventData.m_fValue.Float = 0.0f;
		m_EventData.m_value2.Int = value2;
		safecpy(m_EventData.m_stringValue, stringData);
	}

	CEventNetworkPresence_StatUpdate(const u32 statHash, const char* fromGamer, float value, int value2, const char* stringData)
	{
		m_EventData.m_statNameHash.Int = statHash;
		safecpy(m_EventData.m_fromGamer, fromGamer);
		m_EventData.m_fValue.Float = value;
		m_EventData.m_iValue.Int = 0;
		m_EventData.m_value2.Int = value2;
		safecpy(m_EventData.m_stringValue, stringData);
	}

	CEventNetworkPresence_StatUpdate(const sGamePresenceEvent_StatUpdate& eventData) :
	CEventNetworkWithData<CEventNetworkPresence_StatUpdate, EVENT_NETWORK_PRESENCE_STAT_UPDATE, sGamePresenceEvent_StatUpdate>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("... statNameHash: \"%d\"", m_EventData.m_statNameHash.Int);
		SpewToTTY("....fromGamer: %s", m_EventData.m_fromGamer);
		SpewToTTY("... iValue: \"%d\"", m_EventData.m_iValue.Int);
		SpewToTTY("... fValue: \"%5.2f\"", m_EventData.m_fValue.Float);
		SpewToTTY("... value2: \"%d\"", m_EventData.m_value2.Int);
		SpewToTTY("....stringValue: \"%s\"", m_EventData.m_stringValue);
	}
};

// ================================================================================================================
// CEventNetwork_InboxMsgReceived - triggered when new messages have been received in the inbox
// ================================================================================================================
class CEventNetwork_InboxMsgReceived : public CEventNetworkNoData<CEventNetwork_InboxMsgReceived, EVENT_NETWORK_INBOX_MSGS_RCVD>
{

};

// ================================================================================================================
// CEventNetworkPedLeftBehind
// ================================================================================================================
class CEventNetworkPedLeftBehind : public CEventNetworkWithData<CEventNetworkPedLeftBehind, EVENT_NETWORK_PED_LEFT_BEHIND, sPedLeftBehindData>
{
public:
	enum eReasonCodes
	{
		REASON_DEFAULT
		,REASON_PLAYER_LEFT_SESSION
	};

public:
	CEventNetworkPedLeftBehind(CPed* ped, const u32 reason = REASON_DEFAULT);

	CEventNetworkPedLeftBehind(const sPedLeftBehindData& eventData) : CEventNetworkWithData<CEventNetworkPedLeftBehind, EVENT_NETWORK_PED_LEFT_BEHIND, sPedLeftBehindData>(eventData)
	{
	}

	void SpewEventInfo() const;
	virtual bool  RetrieveData(u8* data, const unsigned sizeOfData) const;

	virtual bool operator==(const fwEvent& event) const 
	{ 
		if (CEventNetworkWithData<CEventNetworkPedLeftBehind, EVENT_NETWORK_PED_LEFT_BEHIND, sPedLeftBehindData>::operator==(event))
		{
			Assert(dynamic_cast<const CEventNetworkPedLeftBehind*>(&event));

			const CEventNetworkPedLeftBehind* pEvent = static_cast<const CEventNetworkPedLeftBehind*>(&event);

			if (pEvent->m_pPed.Get() == m_pPed.Get())
			{
				return true;
			}
		}

		return false;
	}


private:
	fwEvent* CloneInternal() const;

private:
	RegdEnt m_pPed;
	u32     m_Reason;

#if __ASSERT
	ObjectId m_PedId;
#endif
};

// ================================================================================================================
// CEventNetworkSessionEvent - 
// ================================================================================================================
class CEventNetworkSessionEvent : public CEventNetworkWithData<CEventNetworkSessionEvent, EVENT_NETWORK_SESSION_EVENT, sSessionEventData>
{
public:

	enum eEventID
	{
		EVENT_MIGRATE_END
	};

	CEventNetworkSessionEvent(eEventID nEventID, int nEventParam = 0)
	{
		m_EventData.m_nEventID.Int = static_cast<int>(nEventID);
		m_EventData.m_nEventParam.Int = nEventParam;
		for(int i = 0; i < SCRIPT_GAMER_HANDLE_SIZE; i++)
			m_EventData.m_hGamer[i].Int = 0;
	}

	CEventNetworkSessionEvent(eEventID nEventID, const rlGamerHandle& hGamer, int nEventParam = 0)
	{
		hGamer.Export(m_EventData.m_hGamer, sizeof(m_EventData.m_hGamer));
		m_EventData.m_nEventID.Int = static_cast<int>(nEventID);
		m_EventData.m_nEventParam.Int = nEventParam;
	}

	CEventNetworkSessionEvent(const sSessionEventData& eventData) : 
	CEventNetworkWithData<CEventNetworkSessionEvent, EVENT_NETWORK_SESSION_EVENT, sSessionEventData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("... Event ID: %d", m_EventData.m_nEventID);
		SpewToTTY("... Param: %d", m_EventData.m_nEventParam);
	}
};

// ================================================================================================================
// CEventNetworkTransitionStarted - 
// ================================================================================================================
class CEventNetworkTransitionStarted : public CEventNetworkWithData<CEventNetworkTransitionStarted, EVENT_NETWORK_TRANSITION_STARTED, sTransitionStartedData>
{
public:

	CEventNetworkTransitionStarted(bool bSuccess)
	{
		m_EventData.m_bSuccess.Int = bSuccess ? TRUE : FALSE;
	}

	CEventNetworkTransitionStarted(const sTransitionStartedData& eventData) : 
	CEventNetworkWithData<CEventNetworkTransitionStarted, EVENT_NETWORK_TRANSITION_STARTED, sTransitionStartedData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("... Success: %s", m_EventData.m_bSuccess.Int ? "True" : "False");
	}
};

// ================================================================================================================
// CEventNetworkTransitionEvent - 
// ================================================================================================================
class CEventNetworkTransitionEvent : public CEventNetworkWithData<CEventNetworkTransitionEvent, EVENT_NETWORK_TRANSITION_EVENT, sTransitionEventData>
{
public:

	enum eEventID
	{
		EVENT_HOSTING,
		EVENT_HOSTED,
		EVENT_HOST_FAILED,
		EVENT_JOINING,
		EVENT_JOINED,
		EVENT_JOINED_LAUNCHED,
		EVENT_JOIN_FAILED,
		EVENT_STARTING,
		EVENT_STARTED,
		EVENT_ENDING,
		EVENT_ENDED,
		EVENT_QUICKMATCH,
		EVENT_QUICKMATCH_JOINING,
		EVENT_QUICKMATCH_HOSTING,
		EVENT_START_LAUNCH,
		EVENT_END_LAUNCH,
		EVENT_KICKED,
		EVENT_ERROR,
		EVENT_MIGRATE_START,
		EVENT_MIGRATE_END,
		EVENT_TO_GAME_START,
		EVENT_TO_GAME_JOINING,
		EVENT_TO_GAME_JOINED,
		EVENT_TO_GAME_HOSTING,
		EVENT_TO_GAME_FINISH,
		EVENT_QUICKMATCH_FAILED,
        EVENT_QUICKMATCH_TIME_OUT,
        EVENT_GROUP_QUICKMATCH_STARTED,
        EVENT_GROUP_QUICKMATCH_FINISHED,
        EVENT_NUM,
    };

	CEventNetworkTransitionEvent(eEventID nEventID, int nEventParam = 0)
	{
		m_EventData.m_nEventID.Int = static_cast<int>(nEventID);
		m_EventData.m_nEventParam.Int = nEventParam;
        for(int i = 0; i < SCRIPT_GAMER_HANDLE_SIZE; i++)
            m_EventData.m_hGamer[i].Int = 0;
	}

    CEventNetworkTransitionEvent(eEventID nEventID, const rlGamerHandle& hGamer, int nEventParam = 0)
    {
        hGamer.Export(m_EventData.m_hGamer, sizeof(m_EventData.m_hGamer));
        m_EventData.m_nEventID.Int = static_cast<int>(nEventID);
        m_EventData.m_nEventParam.Int = nEventParam;
    }

	CEventNetworkTransitionEvent(const sTransitionEventData& eventData) : 
	CEventNetworkWithData<CEventNetworkTransitionEvent, EVENT_NETWORK_TRANSITION_EVENT, sTransitionEventData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
#if !__NO_OUTPUT
		static const char* s_EventNames[EVENT_NUM] = 
		{
			"EVENT_HOSTING",
			"EVENT_HOSTED",
			"EVENT_HOST_FAILED",
			"EVENT_JOINING",
			"EVENT_JOINED",
			"EVENT_JOINED_LAUNCHED",
			"EVENT_JOIN_FAILED",
			"EVENT_STARTING",
			"EVENT_STARTED",
			"EVENT_ENDING",
			"EVENT_ENDED",
			"EVENT_QUICKMATCH",
			"EVENT_QUICKMATCH_JOINING",
			"EVENT_QUICKMATCH_HOSTING",
			"EVENT_START_LAUNCH",
			"EVENT_END_LAUNCH",
			"EVENT_KICKED",
			"EVENT_ERROR",
			"EVENT_MIGRATE_START",
			"EVENT_MIGRATE_END",
			"EVENT_TO_GAME_START",
			"EVENT_TO_GAME_JOINING",
			"EVENT_TO_GAME_JOINED",
			"EVENT_TO_GAME_HOSTING",
			"EVENT_TO_GAME_FINISH",
			"EVENT_QUICKMATCH_FAILED",
			"EVENT_QUICKMATCH_TIME_OUT",
			"EVENT_GROUP_QUICKMATCH_STARTED",
			"EVENT_GROUP_QUICKMATCH_FINISHED",
		};
#endif

		SpewToTTY("... Event ID: %s", s_EventNames[m_EventData.m_nEventID.Int]);
		SpewToTTY("... Param: %d", m_EventData.m_nEventParam);
	}
};

// ================================================================================================================
// CEventNetworkTransitionMemberJoined - 
// ================================================================================================================
class CEventNetworkTransitionMemberJoined : public CEventNetworkWithData<CEventNetworkTransitionMemberJoined, EVENT_NETWORK_TRANSITION_MEMBER_JOINED, sTransitionMemberData>
{
public:

	CEventNetworkTransitionMemberJoined(const rlGamerHandle& hGamer, const char* szName, const int nCharacterRank)
	{
		hGamer.Export(m_EventData.m_hGamer, sizeof(m_EventData.m_hGamer));
		safecpy(m_EventData.m_szGamerTag, szName);
        m_EventData.m_nCharacterRank.Int = nCharacterRank;
	}

	CEventNetworkTransitionMemberJoined(const sTransitionMemberData& eventData) : 
	CEventNetworkWithData<CEventNetworkTransitionMemberJoined, EVENT_NETWORK_TRANSITION_MEMBER_JOINED, sTransitionMemberData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("... name: %s", m_EventData.m_szGamerTag);
        SpewToTTY("... rank: %d", m_EventData.m_nCharacterRank.Int);
    }
};

// ================================================================================================================
// CEventNetworkTransitionMemberLeft - 
// ================================================================================================================
class CEventNetworkTransitionMemberLeft : public CEventNetworkWithData<CEventNetworkTransitionMemberLeft, EVENT_NETWORK_TRANSITION_MEMBER_LEFT, sTransitionMemberData>
{
public:

	CEventNetworkTransitionMemberLeft(const rlGamerHandle& hGamer, const char* szName)
	{
		hGamer.Export(m_EventData.m_hGamer, sizeof(m_EventData.m_hGamer));
		safecpy(m_EventData.m_szGamerTag, szName);
        m_EventData.m_nCharacterRank.Int = 0;
    }

	CEventNetworkTransitionMemberLeft(const sTransitionMemberData& eventData) : 
	CEventNetworkWithData<CEventNetworkTransitionMemberLeft, EVENT_NETWORK_TRANSITION_MEMBER_LEFT, sTransitionMemberData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("... name: %s", m_EventData.m_szGamerTag);
	}
};

// ================================================================================================================
// CEventNetworkTransitionParameterChanged - 
// ================================================================================================================
class CEventNetworkTransitionParameterChanged : public CEventNetworkWithData<CEventNetworkTransitionParameterChanged, EVENT_NETWORK_TRANSITION_PARAMETER_CHANGED, sTransitionParameterData>
{
public:

	CEventNetworkTransitionParameterChanged(const rlGamerHandle& hGamer, const int numValues, sParameterData (&paramData)[sTransitionParameterData::MAX_NUM_PARAMS])
	{
		hGamer.Export(m_EventData.m_hGamer, sizeof(m_EventData.m_hGamer));

		m_EventData.m_nNumberValues.Int = numValues;

		m_EventData.m_PADDING_SizeOfColArray.Int = sTransitionParameterData::MAX_NUM_PARAMS;

		for (int i=0; i<numValues; i++)
		{
			m_EventData.m_nParamData[i].m_id.Int    = paramData[i].m_id.Int;
			m_EventData.m_nParamData[i].m_value.Int = paramData[i].m_value.Int;
		}
	}

	CEventNetworkTransitionParameterChanged(const sTransitionParameterData& eventData) : 
	CEventNetworkWithData<CEventNetworkTransitionParameterChanged, EVENT_NETWORK_TRANSITION_PARAMETER_CHANGED, sTransitionParameterData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		for (int i=0; i<m_EventData.m_nNumberValues.Int; i++)
		{
			SpewToTTY("... Param index: %d", i);
			SpewToTTY("........ ID: %d", m_EventData.m_nParamData[i].m_id.Int);
			SpewToTTY("........ Value: %d", m_EventData.m_nParamData[i].m_value.Int);
		}
	}
};

// ================================================================================================================
// CEventNetworkTransitionStringChanged - 
// ================================================================================================================
class CEventNetworkTransitionStringChanged : public CEventNetworkWithData<CEventNetworkTransitionStringChanged, EVENT_NETWORK_TRANSITION_STRING_CHANGED, sTransitionStringData>
{
public:

	CEventNetworkTransitionStringChanged(const rlGamerHandle& hGamer, int nID, const char* szParameter)
	{
		hGamer.Export(m_EventData.m_hGamer, sizeof(m_EventData.m_hGamer));
		m_EventData.m_nParameterID.Int = nID;
		safecpy(m_EventData.m_szParameter, szParameter);
	}

	CEventNetworkTransitionStringChanged(const sTransitionStringData& eventData) : 
	CEventNetworkWithData<CEventNetworkTransitionStringChanged, EVENT_NETWORK_TRANSITION_STRING_CHANGED, sTransitionStringData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("... ID: %d", m_EventData.m_nParameterID.Int);
		SpewToTTY("... Parameter: %s", m_EventData.m_szParameter);
	}
};

// ================================================================================================================
// CEventNetworkTransitionGamerInstruction - 
// ================================================================================================================
class CEventNetworkTransitionGamerInstruction : public CEventNetworkWithData<CEventNetworkTransitionGamerInstruction, EVENT_NETWORK_TRANSITION_GAMER_INSTRUCTION, sTransitionGamerInstructionData>
{
public:

	CEventNetworkTransitionGamerInstruction(const rlGamerHandle& hFromGamer, const rlGamerHandle& hToGamer, const char* szGamerTag, int nInstruction, int nInstructionParam)
	{
		hFromGamer.Export(m_EventData.m_hFromGamer, sizeof(m_EventData.m_hFromGamer));
		hToGamer.Export(m_EventData.m_hToGamer, sizeof(m_EventData.m_hToGamer));
        safecpy(m_EventData.m_szGamerTag, szGamerTag);
		m_EventData.m_nInstruction.Int = nInstruction;
		m_EventData.m_nInstructionParam.Int = nInstructionParam;
	}

	CEventNetworkTransitionGamerInstruction(const sTransitionGamerInstructionData& eventData) : 
	CEventNetworkWithData<CEventNetworkTransitionGamerInstruction, EVENT_NETWORK_TRANSITION_GAMER_INSTRUCTION, sTransitionGamerInstructionData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
        SpewToTTY("... GamerName: %s", m_EventData.m_szGamerTag);
		SpewToTTY("... Instruction: %d", m_EventData.m_nInstruction.Int);
		SpewToTTY("... Param: %d", m_EventData.m_nInstructionParam.Int);
	}
};

// ================================================================================================================
// CEventNetworkPresenceInvite - 
// ================================================================================================================
class CEventNetworkPresenceInvite : public CEventNetworkWithData<CEventNetworkPresenceInvite, EVENT_NETWORK_PRESENCE_INVITE, sPresenceInviteData>
{
public:

	CEventNetworkPresenceInvite(const rlGamerHandle& hGamer, const char* szGamer, const char* szContentID, unsigned nPlaylistLength, unsigned nPlaylistCurrent, bool bScTv, int nInviteID)
	{
        if(hGamer.IsValid())
		    hGamer.Export(m_EventData.m_hGamer, sizeof(m_EventData.m_hGamer));
		safecpy(m_EventData.m_szGamerTag, szGamer);
		safecpy(m_EventData.m_szContentID, szContentID);
		m_EventData.m_nPlaylistLength.Int = static_cast<int>(nPlaylistLength);
		m_EventData.m_nPlaylistCurrent.Int = static_cast<int>(nPlaylistCurrent);
		m_EventData.m_bScTv.Int = bScTv ? TRUE : FALSE;
		m_EventData.m_nInviteID.Int = static_cast<int>(nInviteID);
	}

	CEventNetworkPresenceInvite(const sPresenceInviteData& eventData) : 
	CEventNetworkWithData<CEventNetworkPresenceInvite, EVENT_NETWORK_PRESENCE_INVITE, sPresenceInviteData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("... Inviter: %s", m_EventData.m_szGamerTag);
		SpewToTTY("... ContentID: %s", m_EventData.m_szContentID);
		SpewToTTY("... PlaylistLength: %d", m_EventData.m_nPlaylistLength.Int);
		SpewToTTY("... PlaylistCurrent: %d", m_EventData.m_nPlaylistCurrent.Int);
		SpewToTTY("... ScTv: %s", m_EventData.m_bScTv.Int == TRUE ? "True" : "False");
		SpewToTTY("... InviteID: %d", m_EventData.m_nInviteID.Int);
	}
};

// ================================================================================================================
// CEventNetworkPresenceInviteRemoved - 
// ================================================================================================================
class CEventNetworkPresenceInviteRemoved : public CEventNetworkWithData<CEventNetworkPresenceInviteRemoved, EVENT_NETWORK_PRESENCE_INVITE_REMOVED, sPresenceInviteRemovedData>
{
public:

	CEventNetworkPresenceInviteRemoved(int nInviteID, int nReason)
	{
		m_EventData.m_nInviteID.Int = static_cast<int>(nInviteID);
		m_EventData.m_nReason.Int = static_cast<int>(nReason);
	}

	CEventNetworkPresenceInviteRemoved(const sPresenceInviteRemovedData& eventData) : 
	CEventNetworkWithData<CEventNetworkPresenceInviteRemoved, EVENT_NETWORK_PRESENCE_INVITE_REMOVED, sPresenceInviteRemovedData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("... InviteID: %d", m_EventData.m_nInviteID.Int);
		SpewToTTY("... Reason: %d", m_EventData.m_nReason.Int);
	}
};

// ================================================================================================================
// CEventNetworkPresenceInviteReply - 
// ================================================================================================================
class CEventNetworkPresenceInviteReply : public CEventNetworkWithData<CEventNetworkPresenceInviteReply, EVENT_NETWORK_PRESENCE_INVITE_REPLY, sInviteReplyData>
{
public:

    CEventNetworkPresenceInviteReply(const rlGamerHandle& hGamer, const unsigned nStatus, const int nCharacterRank, const char* szClanTag, const bool bDecisionMade, const bool bDecision)
    {
        hGamer.Export(m_EventData.m_hInvitee, sizeof(m_EventData.m_hInvitee));
        m_EventData.m_nStatus.Int = static_cast<int>(nStatus);
        m_EventData.m_nCharacterRank.Int = nCharacterRank;
        safecpy(m_EventData.m_ClanTag, szClanTag);
        m_EventData.m_bDecisionMade.Int = bDecisionMade ? TRUE : FALSE;
        m_EventData.m_bDecision.Int = bDecision ? TRUE : FALSE;
    }

    CEventNetworkPresenceInviteReply(const sInviteReplyData& eventData) : 
    CEventNetworkWithData<CEventNetworkPresenceInviteReply, EVENT_NETWORK_PRESENCE_INVITE_REPLY, sInviteReplyData>(eventData)
    {
    }

    void SpewEventInfo() const
    {
        SpewToTTY("... m_nStatus: %d", m_EventData.m_nStatus.Int);
        SpewToTTY("... m_nCharacterRank: %d", m_EventData.m_nCharacterRank.Int);
        SpewToTTY("... m_szClanTag: %d", m_EventData.m_nCharacterRank.Int);
        SpewToTTY("... m_bDecisionMade: %s", m_EventData.m_bDecisionMade.Int == TRUE ? "True" : "False");
        SpewToTTY("... m_bDecision: %s", m_EventData.m_bDecision.Int == TRUE ? "True" : "False");
    }
};

// ================================================================================================================
// CEventNetworkCashTransactionLog - Triggered when a cash transaction is done with details about the transaction.
// ================================================================================================================
class CEventNetworkCashTransactionLog : public CEventNetworkWithData<CEventNetworkCashTransactionLog, EVENT_NETWORK_CASH_TRANSACTION_LOG, sCashTransactionLog>
{

public:
	enum eTransactionTypes
	{
		 CASH_TRANSACTION_DEPOSIT
		,CASH_TRANSACTION_WITHDRAW
	};

	enum eTransactionIds
	{
		 CASH_TRANSACTION_ATM   //Money deposited/withdrawn by the player - this is relation to VC commands - So deposit or withdraw from BANk Account.
		,CASH_TRANSACTION_GAMER //Money received/paid from friends.
		,CASH_TRANSACTION_STORE //STORE money always credits the BANK account
		,CASH_TRANSACTION_BANK  //BANK account transaction
	};


public:

	CEventNetworkCashTransactionLog(int transactionId, eTransactionIds id, eTransactionTypes type, s64 amount, const rlGamerHandle& hGamer, int itemId = -1);

	CEventNetworkCashTransactionLog(const sCashTransactionLog& eventData) : 
	CEventNetworkWithData<CEventNetworkCashTransactionLog, EVENT_NETWORK_CASH_TRANSACTION_LOG, sCashTransactionLog>(eventData)
	{
	}

	virtual bool operator==(const fwEvent&) const
	{ 
		return false;
	}


	void SpewEventInfo() const
	{
		SpewToTTY("... m_TransactionId: %d", m_EventData.m_TransactionId.Int);
		SpewToTTY("... m_Id: %d", m_EventData.m_Id.Int);
		SpewToTTY("... m_Type: %d", m_EventData.m_Type.Int);
		SpewToTTY("... m_Amount: %d", m_EventData.m_Amount.Int);
		SpewToTTY("... m_ItemId: %d", m_EventData.m_ItemId.Int);
		SpewToTTY("... m_Amount: %s", m_EventData.m_StrAmount);
	}
};


// ================================================================================================================
// CEventNetworkVehicleUndrivable - triggered when a vehicle becomes undrivable.
// ================================================================================================================
class CEventNetworkVehicleUndrivable : public CEventNetworkWithData<CEventNetworkVehicleUndrivable, EVENT_NETWORK_VEHICLE_UNDRIVABLE, sVehicleUndrivableData>
{
public:

	CEventNetworkVehicleUndrivable(CEntity* vehicle, CEntity* damager, const int weaponHash);

	CEventNetworkVehicleUndrivable(const sVehicleUndrivableData& eventData) : 
	CEventNetworkWithData<CEventNetworkVehicleUndrivable, EVENT_NETWORK_VEHICLE_UNDRIVABLE, sVehicleUndrivableData>(eventData)
	{
	}

	void SpewEventInfo() const;

private:
	virtual fwEvent* CloneInternal() const;

#if !__FINAL
	RegdEnt m_pVehicle;
	RegdEnt m_pDamager;
#endif
};

// ================================================================================================================
// CEventNetworkPresenceTriggerEvent - Triggered by a game trigger presence message. 
// ================================================================================================================
class CEventNetworkPresenceTriggerEvent : 
    public CEventNetworkWithData<CEventNetworkPresenceTriggerEvent, EVENT_NETWORK_PRESENCE_TRIGGER, sGamePresenceEventTrigger>
{
public:
    CEventNetworkPresenceTriggerEvent(const u32 nEventNameHash, int nEventParam1, int nEventParam2, const char* szEventString)
    {
        m_EventData.m_nEventNameHash.Int = nEventNameHash;
        m_EventData.m_nEventParam1.Int = nEventParam1;
        m_EventData.m_nEventParam2.Int = nEventParam2;
        safecpy(m_EventData.m_nEventString, szEventString);
    }

    CEventNetworkPresenceTriggerEvent(const sGamePresenceEventTrigger& eventData) :
    CEventNetworkWithData<CEventNetworkPresenceTriggerEvent, EVENT_NETWORK_PRESENCE_TRIGGER, sGamePresenceEventTrigger>(eventData)
    {
    }

    void SpewEventInfo() const
    {
        SpewToTTY("... m_nEventNameHash: \"%d\"", m_EventData.m_nEventNameHash.Int);
        SpewToTTY("... m_nEventParam1: \"%d\"", m_EventData.m_nEventParam1.Int);
        SpewToTTY("... m_nEventParam2: \"%d\"", m_EventData.m_nEventParam2.Int);
        SpewToTTY("....m_nEventString: \"%s\"", m_EventData.m_nEventString);
    }
};

// ================================================================================================================
// CEventNetworkEmailReceivedEvent - Triggered when an email arrives for the local player
// ================================================================================================================
class CEventNetworkEmailReceivedEvent : public CEventNetworkNoData<CEventNetworkEmailReceivedEvent, EVENT_NETWORK_EMAIL_RECEIVED>
{
};

// ================================================================================================================
// CEventNetworkEmailReceivedEvent - Triggered when an email arrives for the local player
// ================================================================================================================
class CEventNetworkMarketingEmailReceivedEvent : public CEventNetworkNoData<CEventNetworkMarketingEmailReceivedEvent, EVENT_NETWORK_MARKETING_EMAIL_RECEIVED>
{
};

// ================================================================================================================
// CEventNetworkFollowInviteReceived - Triggered when we receive a follow invite
// ================================================================================================================
class CEventNetworkFollowInviteReceived : public CEventNetworkWithData<CEventNetworkFollowInviteReceived, EVENT_NETWORK_FOLLOW_INVITE_RECEIVED, sFollowInviteData>
{
public:

    CEventNetworkFollowInviteReceived(const rlGamerHandle& hGamer)
    {
        hGamer.Export(m_EventData.hGamer, sizeof(m_EventData.hGamer));
    }

    CEventNetworkFollowInviteReceived(const sFollowInviteData& eventData) : 
    CEventNetworkWithData<CEventNetworkFollowInviteReceived, EVENT_NETWORK_FOLLOW_INVITE_RECEIVED, sFollowInviteData>(eventData)
    {
    }

    void SpewEventInfo() const
    {

    }
};

// ================================================================================================================
// CEventNetworkAdminInvited - Triggered when an admin is being invited
// ================================================================================================================
class CEventNetworkAdminInvited : public CEventNetworkWithData<CEventNetworkAdminInvited, EVENT_NETWORK_ADMIN_INVITED, sAdminInviteData>
{
public:

    CEventNetworkAdminInvited(const rlGamerHandle& hGamer, const bool bIsSCTV)
    {
        hGamer.Export(m_EventData.hGamer, sizeof(m_EventData.hGamer));
        m_EventData.bIsSCTV.Int = bIsSCTV ? 1 : 0;
    }

    CEventNetworkAdminInvited(const sAdminInviteData& eventData) : 
    CEventNetworkWithData<CEventNetworkAdminInvited, EVENT_NETWORK_ADMIN_INVITED, sAdminInviteData>(eventData)
    {
    }

    void SpewEventInfo() const
    {
        SpewToTTY("... SCTV: %s", m_EventData.bIsSCTV.Int == 1 ? "True" : "False");
    }
};

// ================================================================================================================
// CEventNetworkSpectateLocal - 
// ================================================================================================================
class CEventNetworkSpectateLocal : public CEventNetworkWithData<CEventNetworkSpectateLocal, EVENT_NETWORK_SPECTATE_LOCAL, sSpectateLocalData>
{
public:

    CEventNetworkSpectateLocal(const rlGamerHandle& hGamer)
    {
        hGamer.Export(m_EventData.hGamer, sizeof(m_EventData.hGamer));
    }

    CEventNetworkSpectateLocal(const sSpectateLocalData& eventData) : 
    CEventNetworkWithData<CEventNetworkSpectateLocal, EVENT_NETWORK_SPECTATE_LOCAL, sSpectateLocalData>(eventData)
    {
    }

    void SpewEventInfo() const
    {

    }
};

// ================================================================================================================
// CEventNetworkCloudEvent - Triggered by a cloud event
// ================================================================================================================
class CEventNetworkCloudEvent : public CEventNetworkWithData<CEventNetworkCloudEvent, EVENT_NETWORK_CLOUD_EVENT, sCloudEventData>
{
public:

    enum eEventID
    {
        CLOUD_EVENT_TUNABLES_ADDED,
        CLOUD_EVENT_BGSCRIPT_ADDED,
    };

    CEventNetworkCloudEvent(const eEventID nEventID, int nEventParam1 = 0, int nEventParam2 = 0, const char* szEventString = NULL)
    {
        m_EventData.m_nEventID.Int = nEventID;
        m_EventData.m_nEventParam1.Int = nEventParam1;
        m_EventData.m_nEventParam2.Int = nEventParam2;

        if(szEventString)
            safecpy(m_EventData.m_nEventString, szEventString);
        else
            safecpy(m_EventData.m_nEventString, "");
    }

    CEventNetworkCloudEvent(const sCloudEventData& eventData) :
    CEventNetworkWithData<CEventNetworkCloudEvent, EVENT_NETWORK_CLOUD_EVENT, sCloudEventData>(eventData)
    {
    }

    void SpewEventInfo() const
    {
        SpewToTTY("... m_nEventID: \"%d\"", m_EventData.m_nEventID.Int);
        SpewToTTY("... m_nEventParam1: \"%d\"", m_EventData.m_nEventParam1.Int);
        SpewToTTY("... m_nEventParam2: \"%d\"", m_EventData.m_nEventParam2.Int);
        SpewToTTY("....m_nEventString: \"%s\"", m_EventData.m_nEventString);
    }
};

// CNetworkShopTransactionEvent - Triggered when a network shop transaction is done
// ================================================================================================================
class CEventNetworkShopTransaction : public CEventNetworkWithData<CEventNetworkShopTransaction, EVENT_NETWORK_SHOP_TRANSACTION, sNetworkShopTransaction>
{
public:
	CEventNetworkShopTransaction(const int id, const int type, const int action, const int resultcode, const int serviceid, const int bank, const int wallet)
	{
		m_EventData.m_Id.Int = id;
		m_EventData.m_Type.Int = type;
		m_EventData.m_Action.Int = action;
		m_EventData.m_ResultCode.Int = resultcode;
		m_EventData.m_Serviceid.Int = serviceid;
		m_EventData.m_Bank.Int = bank;
		m_EventData.m_Wallet.Int = wallet;
	}

	CEventNetworkShopTransaction(const sNetworkShopTransaction& eventData) 
		: CEventNetworkWithData<CEventNetworkShopTransaction, EVENT_NETWORK_SHOP_TRANSACTION, sNetworkShopTransaction>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("... m_Id: \"0x%08X\"", static_cast<u32>(m_EventData.m_Id.Int));
		SpewToTTY("... m_Type: \"0x%08X\"", static_cast<u32>(m_EventData.m_Type.Int));
		SpewToTTY("... m_Action: \"0x%08X\"", static_cast<u32>(m_EventData.m_Action.Int));
		SpewToTTY("... m_ResultCode: \"%d\"", m_EventData.m_ResultCode.Int);
		SpewToTTY("... m_Serviceid: \"%d\"", m_EventData.m_Serviceid.Int);
		SpewToTTY("... m_Bank: \"%d\"", m_EventData.m_Bank.Int);
		SpewToTTY("... m_Wallet: \"%d\"", m_EventData.m_Wallet.Int);
	}
};

// ================================================================================================================
// CEventNetworkPermissionCheckResult - triggered when we complete a permissions check
// ================================================================================================================
class CEventNetworkPermissionCheckResult : public CEventNetworkWithData<CEventNetworkPermissionCheckResult, EVENT_NETWORK_PERMISSION_CHECK_RESULT, sPermissionCheckResult>
{
public:
	CEventNetworkPermissionCheckResult(const int nCheckID, bool bAllowed)
	{
		m_EventData.m_nCheckID.Int = nCheckID;
		m_EventData.m_bAllowed.Int = bAllowed ? TRUE : FALSE;
	}

	CEventNetworkPermissionCheckResult(const sPermissionCheckResult& eventData) 
		: CEventNetworkWithData<CEventNetworkPermissionCheckResult, EVENT_NETWORK_PERMISSION_CHECK_RESULT, sPermissionCheckResult>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("... m_nCheckID: %d", m_EventData.m_nCheckID.Int);
		SpewToTTY("... m_bAllowed: %s", m_EventData.m_bAllowed.Int == TRUE ? "True" : "False");
	}
};

// ================================================================================================================
// CEventNetworkOnlinePermissionsUpdated - triggered when online permissions are updated
// ================================================================================================================
class CEventNetworkOnlinePermissionsUpdated : public CEventNetworkNoData<CEventNetworkOnlinePermissionsUpdated, EVENT_NETWORK_ONLINE_PERMISSIONS_UPDATED>
{
};

// ================================================================================================================
// CNetworkShopTransactionEvent - Triggered when a social club account is linked
// ================================================================================================================
class CEventNetworkSocialClubAccountLinked : public CEventNetworkNoData<CEventNetworkSocialClubAccountLinked, EVENT_NETWORK_SOCIAL_CLUB_ACCOUNT_LINKED>
{
};



// ================================================================================================================
// CEventNetworkAppLaunched - Triggered when we receive an app launched system event
// ================================================================================================================
class CEventNetworkAppLaunched : public CEventNetworkWithData<CEventNetworkAppLaunched, EVENT_NETWORK_APP_LAUNCHED, sAppLaunchedData>
{
public:
	CEventNetworkAppLaunched(const unsigned nFlags)
	{
		m_EventData.m_nFlags.Int = static_cast<int>(nFlags);
	}

	CEventNetworkAppLaunched(const sAppLaunchedData& eventData) 
		: CEventNetworkWithData<CEventNetworkAppLaunched, EVENT_NETWORK_APP_LAUNCHED, sAppLaunchedData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("... m_nFlags: %d", m_EventData.m_nFlags.Int);
	}
};

// ================================================================================================================
// CEventNetworkSystemServiceEvent - Triggered when we receive an system event
// ================================================================================================================
class CEventNetworkSystemServiceEvent : public CEventNetworkWithData<CEventNetworkSystemServiceEvent, EVENT_NETWORK_SYSTEM_SERVICE_EVENT, sSystemServiceEventData>
{
public:

	CEventNetworkSystemServiceEvent(const int nEventID, const unsigned nFlags)
	{
		m_EventData.m_nEventID.Int = nEventID;
		m_EventData.m_nFlags.Int = static_cast<int>(nFlags);
	}

	CEventNetworkSystemServiceEvent(const sSystemServiceEventData& eventData) 
		: CEventNetworkWithData<CEventNetworkSystemServiceEvent, EVENT_NETWORK_SYSTEM_SERVICE_EVENT, sSystemServiceEventData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("... m_nEventID: %d", m_EventData.m_nEventID.Int);
		SpewToTTY("... m_nFlags: %d", m_EventData.m_nFlags.Int);
	}
};

// CEventNetworkRequestDelay - Triggered when there is a service access request delay due to the conductor doing a GEO_MOVE
// ================================================================================================================
class CEventNetworkRequestDelay : public CEventNetworkWithData<CEventNetworkRequestDelay, EVENT_NETWORK_REQUEST_DELAY, sNetworkRequestDelayData>
{
public:
	CEventNetworkRequestDelay(const int retryTimeMs)
	{
		m_EventData.m_delayTimeMs.Int = retryTimeMs;
	}

	CEventNetworkRequestDelay(const sNetworkRequestDelayData& eventData) 
		: CEventNetworkWithData<CEventNetworkRequestDelay, EVENT_NETWORK_REQUEST_DELAY, sNetworkRequestDelayData>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("... m_delayTimeMs: \"0x%08X\"", static_cast<u32>(m_EventData.m_delayTimeMs.Int));
	}
};

// CEventNetworkScAdminPlayerUpdated - Triggered when scAdmin updates the player balance/inventory.
// ================================================================================================================
class CEventNetworkScAdminPlayerUpdated : public CEventNetworkWithData<CEventNetworkScAdminPlayerUpdated, EVENT_NETWORK_SCADMIN_PLAYER_UPDATED, sNetworkScAdminPlayerUpdate>
{
public:
	CEventNetworkScAdminPlayerUpdated(const sNetworkScAdminPlayerUpdate* eventData)
	{
		if (gnetVerify(eventData))
		{
			m_EventData.m_fullRefreshRequested.Int = eventData->m_fullRefreshRequested.Int;
			m_EventData.m_updatePlayerBalance.Int  = eventData->m_updatePlayerBalance.Int;
			m_EventData.m_awardTypeHash.Int        = eventData->m_awardTypeHash.Int;
			m_EventData.m_awardAmount.Int        = eventData->m_awardAmount.Int;

			for (int i=0; i<sNetworkScAdminPlayerUpdate::MAX_NUM_ITEMS; i++)
				m_EventData.m_items[i].Int = eventData->m_items[i].Int;
		}
	}

	CEventNetworkScAdminPlayerUpdated(const sNetworkScAdminPlayerUpdate& eventData) 
		: CEventNetworkWithData<CEventNetworkScAdminPlayerUpdated, EVENT_NETWORK_SCADMIN_PLAYER_UPDATED, sNetworkScAdminPlayerUpdate>(eventData)
	{
	}

	void SpewEventInfo() const
	{
		SpewToTTY("..... m_fullRefreshRequested: \"%s\"",  m_EventData.m_fullRefreshRequested.Int > 0 ? "True" : "False");
		SpewToTTY("...... m_updatePlayerBalance: \"%s\"", m_EventData.m_updatePlayerBalance.Int > 0 ? "True" : "False");
		SpewToTTY("............ m_awardTypeHash: \"%d\"",  m_EventData.m_awardTypeHash.Int);
		SpewToTTY(".............. m_awardAmount: \"%d\"",  m_EventData.m_awardAmount.Int);
		for (int i=0; i<sNetworkScAdminPlayerUpdate::MAX_NUM_ITEMS; i++)
			SpewToTTY("................ m_items[%d]: \"%d\"",  i, m_EventData.m_items[i].Int);
	}
};

// CEventNetworkScAdminReceivedCash - Triggered when scAdmin gives cash to a player.
// ================================================================================================================
class CEventNetworkScAdminReceivedCash : public CEventNetworkWithData<CEventNetworkScAdminReceivedCash, EVENT_NETWORK_SCADMIN_RECEIVED_CASH, sNetworkScAdminReceivedCash>
{
public:
    CEventNetworkScAdminReceivedCash(const sNetworkScAdminReceivedCash* eventData)
    {
        if (gnetVerify(eventData))
        {
            m_EventData.m_characterIndex.Int = eventData->m_characterIndex.Int;
            m_EventData.m_cashAmount.Int  = eventData->m_cashAmount.Int;
        }
    }

    CEventNetworkScAdminReceivedCash(const sNetworkScAdminReceivedCash& eventData) 
        : CEventNetworkWithData<CEventNetworkScAdminReceivedCash, EVENT_NETWORK_SCADMIN_RECEIVED_CASH, sNetworkScAdminReceivedCash>(eventData)
    {
    }

    void SpewEventInfo() const
    {
        SpewToTTY("...... m_characterIndex: %d",  m_EventData.m_characterIndex.Int );
        SpewToTTY("...... m_cashAmount: %d $", m_EventData.m_cashAmount.Int );
    }
};

// ================================================================================================================
// CEventNetworkStuntPerformed - Triggered when the player performs a stunt and the stunt tracking has been enabled (see command PLAYSTATS_START_TRACKING_STUNTS/PLAYSTATS_STOP_TRACKING_STUNTS)
// ================================================================================================================
class CEventNetworkStuntPerformed : public CEventNetworkWithData<CEventNetworkStuntPerformed, EVENT_NETWORK_STUNT_PERFORMED, sNetworkStuntPerformed>
{
public:
    CEventNetworkStuntPerformed(const sNetworkStuntPerformed* eventData)
    {
        if (gnetVerify(eventData))
        {
            m_EventData.m_stuntType.Int = eventData->m_stuntType.Int;
            m_EventData.m_stuntValue.Float  = eventData->m_stuntValue.Float;
        }
    }

    CEventNetworkStuntPerformed(const sNetworkStuntPerformed& eventData) 
        : CEventNetworkWithData<CEventNetworkStuntPerformed, EVENT_NETWORK_STUNT_PERFORMED, sNetworkStuntPerformed>(eventData)
    {
    }
    CEventNetworkStuntPerformed(const int stuntType, const float stuntValue)
    {
        m_EventData.m_stuntType.Int = stuntType;
        m_EventData.m_stuntValue.Float  = stuntValue;
    }

    void SpewEventInfo() const
    {
        SpewToTTY("...... m_stuntType: %d",  m_EventData.m_stuntType.Int );
        SpewToTTY("...... m_stuntValue: %f", m_EventData.m_stuntValue.Float );
    }
};

// ================================================================================================================
// CEventNetworkScMembershipStatus - Triggered if membership status changes
// ================================================================================================================
class CEventNetworkScMembershipStatus : public CEventNetworkWithData<CEventNetworkScMembershipStatus, EVENT_NETWORK_SC_MEMBERSHIP_STATUS, sScMembershipData>
{
public:

	enum EventType
	{
		EventType_StatusReceived,
		EventType_StatusChanged,
	};

	CEventNetworkScMembershipStatus(const sScMembershipData* eventData)
	{
		if(gnetVerify(eventData))
		{
			m_EventData.m_EventType.Int = eventData->m_EventType.Int;

			m_EventData.m_MembershipInfo.m_HasMembership.Int = eventData->m_MembershipInfo.m_HasMembership.Int;
			m_EventData.m_MembershipInfo.m_MembershipStartPosix.Int = eventData->m_MembershipInfo.m_MembershipStartPosix.Int;
			m_EventData.m_MembershipInfo.m_MembershipEndPosix.Int = eventData->m_MembershipInfo.m_MembershipEndPosix.Int;

			m_EventData.m_PrevMembershipInfo.m_HasMembership.Int = eventData->m_PrevMembershipInfo.m_HasMembership.Int;
			m_EventData.m_PrevMembershipInfo.m_MembershipStartPosix.Int = eventData->m_PrevMembershipInfo.m_MembershipStartPosix.Int;
			m_EventData.m_PrevMembershipInfo.m_MembershipEndPosix.Int = eventData->m_PrevMembershipInfo.m_MembershipEndPosix.Int;
		}
	}

	CEventNetworkScMembershipStatus(const sScMembershipData& eventData) 
		: CEventNetworkWithData<CEventNetworkScMembershipStatus, EVENT_NETWORK_SC_MEMBERSHIP_STATUS, sScMembershipData>(eventData)
	{
	}

#if NET_ENABLE_MEMBERSHIP_FUNCTIONALITY
	CEventNetworkScMembershipStatus(const EventType eventType, const rlScMembershipInfo& info, const rlScMembershipInfo& prevInfo)
	{
		m_EventData.m_EventType.Int = eventType;

		m_EventData.m_MembershipInfo.m_HasMembership.Int = info.HasMembership() ? 1 : 0;
		m_EventData.m_MembershipInfo.m_MembershipStartPosix.Int = static_cast<int>(info.GetStartPosix());
		m_EventData.m_MembershipInfo.m_MembershipEndPosix.Int = static_cast<int>(info.GetExpirePosix());

		m_EventData.m_PrevMembershipInfo.m_HasMembership.Int = prevInfo.HasMembership() ? 1 : 0;
		m_EventData.m_PrevMembershipInfo.m_MembershipStartPosix.Int = static_cast<int>(prevInfo.GetStartPosix());
		m_EventData.m_PrevMembershipInfo.m_MembershipEndPosix.Int = static_cast<int>(prevInfo.GetExpirePosix());
	}
#endif

	void SpewEventInfo() const
	{
		SpewToTTY("...... EventType: %d", m_EventData.m_EventType.Int);
		SpewToTTY("...... HasMembership: %d", m_EventData.m_MembershipInfo.m_HasMembership.Int);
		SpewToTTY("...... MembershipStart: %d", m_EventData.m_MembershipInfo.m_MembershipStartPosix.Int);
		SpewToTTY("...... MembershipExpiry: %d", m_EventData.m_MembershipInfo.m_MembershipEndPosix.Int);
		SpewToTTY("...... HasMembership (Prev): %d", m_EventData.m_PrevMembershipInfo.m_HasMembership.Int);
		SpewToTTY("...... MembershipStart (Prev): %d", m_EventData.m_PrevMembershipInfo.m_MembershipStartPosix.Int);
		SpewToTTY("...... MembershipExpiry (Prev): %d", m_EventData.m_PrevMembershipInfo.m_MembershipEndPosix.Int);
	}
};

// ================================================================================================================
// CEventNetworkScBenefitsStatus - Triggered for benefits changes / notifications
// ================================================================================================================
class CEventNetworkScBenefitsStatus : public CEventNetworkWithData<CEventNetworkScBenefitsStatus, EVENT_NETWORK_SC_BENEFITS_STATUS, sScBenefitsData>
{
public:

	enum EventType
	{
		EventType_BenefitsClaimed,
	};

	CEventNetworkScBenefitsStatus(const sScBenefitsData* eventData)
	{
		if(gnetVerify(eventData))
		{
			m_EventData.m_EventType.Int = eventData->m_EventType.Int;
			m_EventData.m_NumBenefits.Int = eventData->m_NumBenefits.Int;
			m_EventData.m_BenefitValue.Int = eventData->m_BenefitValue.Int;
		}
	}

	CEventNetworkScBenefitsStatus(const sScBenefitsData& eventData)
		: CEventNetworkWithData<CEventNetworkScBenefitsStatus, EVENT_NETWORK_SC_BENEFITS_STATUS, sScBenefitsData>(eventData)
	{
	}

#if NET_ENABLE_MEMBERSHIP_FUNCTIONALITY
	CEventNetworkScBenefitsStatus(const EventType eventType, const unsigned numBenefits, const unsigned benefitValue)
	{
		m_EventData.m_EventType.Int = eventType;
		m_EventData.m_NumBenefits.Int = numBenefits;
		m_EventData.m_BenefitValue.Int = benefitValue;
	}
#endif

	void SpewEventInfo() const
	{
		SpewToTTY("...... EventType: %d", m_EventData.m_EventType.Int);
		SpewToTTY("...... NumBenefits: %d", m_EventData.m_NumBenefits.Int);
		SpewToTTY("...... BenefitValue: %d", m_EventData.m_BenefitValue.Int);
	}
};


#endif // EVENT_NETWORK_H

// eof

