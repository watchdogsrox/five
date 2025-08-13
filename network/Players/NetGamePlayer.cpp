//
// NetGamePlayer.cpp
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#include "NetGamePlayer.h"

#include "rline/clan/rlclan.h"
#include "rline/rlgamerinfo.h"

#include "network/NetworkInterface.h"
#include "network/arrays/NetworkArrayMgr.h"
#include "network/Network.h"
#include "network/Objects/Synchronisation/SyncTrees/ProjectSyncTrees.h"
#include "network/roaming/RoamingBubbleMgr.h"
#include "Network/Sessions/NetworkSession.h"
#include "Network/Live/livemanager.h"
#include "Network/Live/NetworkClan.h"
#include "peds/PlayerInfo.h"
#include "Peds/ped.h"
#include "scene/ExtraContent.h"
#include "Stats/StatsInterface.h"

// We now have temporary players to bridge the gap between the display name code 
// these take up a CNonPhysicalPlayerData slot, but aren't removed until we add the equivalent player
// This means we will tip over the pool if we are already at 31 players. 
// To be *sure*, just double the pool to account for this. Eventually, we'll punt temporary players.
static const unsigned MAX_NON_PHYSICAL_PLAYER_DATA = MAX_NUM_PHYSICAL_PLAYERS * 2; 

FW_INSTANTIATE_CLASS_POOL(CNonPhysicalPlayerData, MAX_NON_PHYSICAL_PLAYER_DATA, atHashString("CNonPhysicalPlayerData",0x152d2d4));

// ================================================================================================================
// CNonPhysicalPlayerData
// ================================================================================================================
CNonPhysicalPlayerData::CNonPhysicalPlayerData() :
m_position(VEC3_ZERO)
{
}

CNonPhysicalPlayerData::~CNonPhysicalPlayerData()
{
}

CRoamingBubbleMemberInfo& CNonPhysicalPlayerData::GetRoamingBubbleMemberInfo()
{
    return m_RoamingBubbleMemberInfo;
}

const CRoamingBubbleMemberInfo& CNonPhysicalPlayerData::GetRoamingBubbleMemberInfo() const
{
	return m_RoamingBubbleMemberInfo;
}

void CNonPhysicalPlayerData::SetRoamingBubbleMemberInfo(const CRoamingBubbleMemberInfo& info)
{
    m_RoamingBubbleMemberInfo.Copy(info);
}

const Vector3 &CNonPhysicalPlayerData::GetPosition() const
{
    return m_position;
}

void CNonPhysicalPlayerData::SetPosition(const Vector3 &position)
{
    m_position = position;
}

void CNonPhysicalPlayerData::Read(const datBitBuffer &messageBuffer)
{
    CSyncDataReader serialiser((datBitBuffer &)messageBuffer);
    Serialise(serialiser);
}

void CNonPhysicalPlayerData::Write(datBitBuffer &messageBuffer)
{
    CSyncDataWriter serialiser(messageBuffer);
    Serialise(serialiser);
}

unsigned CNonPhysicalPlayerData::GetSize()
{
    CSyncDataSizeCalculator serialiser;
    Serialise(serialiser);
    return serialiser.GetSize();
}

void CNonPhysicalPlayerData::WriteToLogFile(netLoggingInterface &log)
{
    log.WriteDataValue("Bubble ID", "%d", m_RoamingBubbleMemberInfo.GetBubbleID());
    log.WriteDataValue("Player ID", "%d", m_RoamingBubbleMemberInfo.GetPlayerID());
    log.WriteDataValue("Position", "%f, %f, %f", m_position.x, m_position.y, m_position.z);
}

// ================================================================================================================
// CNetGamePlayer
// ================================================================================================================
CNetGamePlayer::CNetGamePlayer() :
m_PlayerInfo(0),
m_PlayerHasJustDied(false),
m_bIsRockstarDev(false),
m_bIsRockstarQA(false),
m_bIsCheater(false),
m_nCharacterRank(0),
m_IsInDifferentTutorialSession(false),
m_nPropertyID(NO_PROPERTY_ID),
m_nMentalState(NO_MENTAL_STATE),
WIN32PC_ONLY(m_fPedDensity(0),)
m_GarageInstanceIndex(INVALID_GARAGE_INDEX),
m_MatchmakingGroup(MM_GROUP_INVALID),
m_bIsSpectator(false),
m_nCrewId(RL_INVALID_CLAN_ID),
m_VehicleWithPhoneExplosive(NETWORK_INVALID_OBJECT_ID),
m_bIsLocallyDisconnected(false),
m_bHasSentTimeoutScriptEvent(false),
m_bWasDisconnectedDuringTransition(false),
m_bIsLaunchingTransition(false),
m_bHasStartedTransition(false),
m_MuteBeenMutedCount(0),
m_MuteTalkersMetCount(0),
m_bGamerDataDirtyPending(false),
m_bHasVoiceProximityOverride(false),
m_vVoiceProximityOverride(VEC3_ZERO),
m_bOverrideSpectatedVehicleRadio(false),
m_bEmblemRefd(false),
m_PlayerFlags(0),
m_bShouldAllocateBubble(true),
m_SessionBailReason(eBailReason::BAIL_INVALID)
{
	m_cheatsTamperCodes.Reset();

	m_TransitionSessionInfo.Clear();
	m_cheatsNotified.Reset();

	m_hasCommunicationPrivileges = false; 
}

CNetGamePlayer::~CNetGamePlayer()
{
}

void CNetGamePlayer::Shutdown()
{
	m_PlayerHasJustDied					= false;
	m_MatchmakingGroup					= MM_GROUP_INVALID;
    m_bIsSpectator						= false;
    m_nCrewId							= RL_INVALID_CLAN_ID;
	m_bIsLocallyDisconnected			= false;
	m_bHasSentTimeoutScriptEvent		= false;
	m_bWasDisconnectedDuringTransition	= false;
	m_bIsLaunchingTransition			= false;
	m_bGamerDataDirtyPending			= false;
	m_nPropertyID						= NO_PROPERTY_ID;
	m_nMentalState						= NO_MENTAL_STATE;
	WIN32PC_ONLY(m_fPedDensity						= 0;)
    m_GarageInstanceIndex				= INVALID_GARAGE_INDEX;
    m_bHasVoiceProximityOverride		= false;
    m_vVoiceProximityOverride			= Vector3(VEC3_ZERO);
	m_bOverrideSpectatedVehicleRadio	= false;
	m_IsInDifferentTutorialSession		= false;
	m_bIsRockstarDev					= false;
	m_bIsRockstarQA						= false;
	m_bIsCheater						= false;
	m_VehicleWithPhoneExplosive			= NETWORK_INVALID_OBJECT_ID;
	m_PlayerFlags						= 0;
		
	if (m_PlayerInfo)
	{
		if (IsMyPlayer())
		{
			m_PlayerInfo->MultiplayerReset();
		}
		else
		{
			if(GetPlayerPed() && (m_PlayerInfo == GetPlayerPed()->GetPlayerInfo()))
			{
				//! gnetVerify that player info has been removed. If not, we must do it now or we'll leave it dangling.
				if(!gnetVerifyf(!GetPlayerPed()->GetPlayerInfo(), "CNetGamePlayer::Shutdown() - Deleting player info pointer (%p) when it is still referenced by %s", m_PlayerInfo, GetPlayerPed()->GetDebugName()))
				{
					CControlledByInfo currentControlInfo = GetPlayerPed()->GetControlledByInfo();
					const CControlledByInfo newControlInfo(currentControlInfo.IsControlledByNetwork(), false);
					GetPlayerPed()->SetControlledByInfo(newControlInfo);
					GetPlayerPed()->SetPlayerInfo(NULL);
				}
			}
			
			//safety net - we MUST unref player info ptr from any peds that point to it.
			{
				// ensure no other peds are using this player info
				CPed::Pool *pedPool = CPed::GetPool();

				for(int index = 0; index < pedPool->GetSize(); index++)
				{
					CPed *pPed = pedPool->GetSlot(index);

					if(pPed && pPed->GetPlayerInfo() == m_PlayerInfo)
					{
						gnetAssertf(0, "CNetGamePlayer::Shutdown() - Safety Net. Deleting a player info pointer (%p) when it is still referenced by %s", m_PlayerInfo, pPed->GetDebugName());
						CControlledByInfo currentControlInfo = pPed->GetControlledByInfo();
						const CControlledByInfo newControlInfo(currentControlInfo.IsControlledByNetwork(), false);
						pPed->SetControlledByInfo(newControlInfo);

						pPed->SetPlayerInfo(NULL);
					}
				}
			}

			delete m_PlayerInfo;	
		}
	}

    m_PlayerInfo = 0;
    m_RoamingBubbleMemberInfo.Invalidate();

    m_nCharacterRank = 0;
	
	//Set the clan info to a cleared clan desc to clear it out correctly.
	rlClanMembershipData blankMemInfo; blankMemInfo.Clear();
	SetClanMembershipInfo(blankMemInfo);

	m_bHasStartedTransition = false;
	m_TransitionSessionInfo.Clear();

	m_MuteBeenMutedCount = 0;
	m_MuteTalkersMetCount = 0;

	m_cheatsNotified.Reset();

	m_hasCommunicationPrivileges = false;

    netPlayer::Shutdown();

	m_cheatsTamperCodes.Reset();

	m_bShouldAllocateBubble = true;
}

void CNetGamePlayer::ActiveUpdate()
{
	// update local player state
	// Online sign in status checked because player lists are updated when removing players. A sign out 
	// triggers a full network shutdown, removing all players. This function will be polled for each player
	// removed and at this time the local gamer index, accessed below, will be -1
	if(IsLocal() && !IsBot() && NetworkInterface::IsLocalPlayerOnline())
	{
		// update the local real players clan info when it changes
		int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		const rlClanMembershipData& primaryClanMembership = rlClan::GetPrimaryMembership(localGamerIndex);

		//Check to see if some stuff changed.
		if (   primaryClanMembership.m_Clan.m_Id != m_clanMembershipInfo.m_Clan.m_Id
			|| primaryClanMembership.m_Rank.m_RankOrder != m_clanMembershipInfo.m_Rank.m_RankOrder 
			|| primaryClanMembership.m_Id != m_clanMembershipInfo.m_Id )
		{
			SetClanMembershipInfo(primaryClanMembership);
		}

		//@@: range CNETGAMEPLAYER_ACTIVEUPDATE_QUALIFYING_PROPERTIES {
		bool bIsRockstarDev = NetworkInterface::IsRockstarDev();
		if(m_bIsRockstarDev != bIsRockstarDev)
		{
			m_bIsRockstarDev = bIsRockstarDev;
			DirtyGamerDataSyncNode();
		}

		bool bIsRockstarQA = NetworkInterface::IsRockstarQA();
		if(m_bIsRockstarQA != bIsRockstarQA)
		{
			m_bIsRockstarQA = bIsRockstarQA;
			DirtyGamerDataSyncNode();
		}

		bool bIsCheater = CNetwork::IsCheater();
		if(m_bIsCheater != bIsCheater)
		{
			m_bIsCheater = bIsCheater;
			DirtyGamerDataSyncNode();
		} 
		//@@: } CNETGAMEPLAYER_ACTIVEUPDATE_QUALIFYING_PROPERTIES

        eMatchmakingGroup nGroup = CNetwork::GetNetworkSession().GetMatchmakingGroup();
        if(nGroup != m_MatchmakingGroup)
        {
            m_MatchmakingGroup = nGroup;
            DirtyGamerDataSyncNode();
        }

        // need to check this each frame (it's cached)
		bool bHavePrivilege = CLiveManager::CheckVoiceCommunicationPrivileges(GAMER_INDEX_EVERYONE);
		if(m_hasCommunicationPrivileges != bHavePrivilege)
		{
			DirtyGamerDataSyncNode();
			m_hasCommunicationPrivileges = bHavePrivilege;
		}

        // if we have pending dirty gamer data
        if(m_bGamerDataDirtyPending)
            DirtyGamerDataSyncNode();

		PlayerAccountId playerAccountId = InvalidPlayerAccountId;
		const rlRosCredentials& cred = rlRos::GetCredentials(NetworkInterface::GetLocalGamerIndex());
		if(cred.IsValid())
		{
			playerAccountId = cred.GetPlayerAccountId();
		}
		if (playerAccountId != m_PlayerAccountId)
		{
			m_PlayerAccountId = playerAccountId;
			DirtyGamerDataSyncNode();
		}
	}
}

void CNetGamePlayer::SetStartedTransition(bool bStarted)
{
	if(m_bHasStartedTransition != bStarted)
	{
		m_bHasStartedTransition = bStarted;
		if(IsLocal())
			DirtyGamerDataSyncNode();
	}
}

void CNetGamePlayer::SetTransitionSessionInfo(const rlSessionInfo& hInfo)
{
	if(m_TransitionSessionInfo != hInfo)
	{
		m_TransitionSessionInfo = hInfo;
		CNetwork::GetNetworkSession().OnReceivedTransitionSessionInfo(this, false);
		if(IsLocal())
			DirtyGamerDataSyncNode();
	}
}

void CNetGamePlayer::ApplyVoiceProximityOverride(Vector3 const& vOverride)
{
    m_bHasVoiceProximityOverride = true; 
    m_vVoiceProximityOverride = vOverride;
}

void CNetGamePlayer::ClearVoiceProximityOverride()
{
    m_bHasVoiceProximityOverride = false; 
    m_vVoiceProximityOverride = Vector3(VEC3_ZERO);
}

Vector3 const& CNetGamePlayer::GetVoiceProximity(Vector3& vPosition) const
{
    vPosition = Vector3(VEC3_ZERO);

    if(m_bHasVoiceProximityOverride)
        vPosition = m_vVoiceProximityOverride;
    else if(GetPlayerPed())
        vPosition = VEC3V_TO_VECTOR3(GetPlayerPed()->GetTransform().GetPosition());

    return vPosition;
}

void CNetGamePlayer::SetOverrideSpectatedVehicleRadio(bool bValue)
{
	bool bReset = false;
	if (m_bOverrideSpectatedVehicleRadio && !bValue)
		bReset = true;

	m_bOverrideSpectatedVehicleRadio = bValue;

	//reset - need to reset the vehicle audio entity also so the spectated player audio is reintroduced.
#if NA_RADIO_ENABLED
	if (bReset)
	{
		CPed* pPlayer = CGameWorld::FindFollowPlayer();
		if (pPlayer && pPlayer->GetMyVehicle() && pPlayer->GetMyVehicle()->GetVehicleAudioEntity())
		{
			pPlayer->GetMyVehicle()->GetVehicleAudioEntity()->ResetRadioStationFromNetwork();
		}
	}
#endif
}

void CNetGamePlayer::InvalidateTransitionSessionInfo()
{
	if(!m_TransitionSessionInfo.IsClear())
	{
		m_TransitionSessionInfo.Clear();
		CNetwork::GetNetworkSession().OnReceivedTransitionSessionInfo(this, true);
		if(IsLocal())
			DirtyGamerDataSyncNode();
	}
}


bool CNetGamePlayer::IsCheatAlreadyNotified(const u32 cheat, const u32 cheatExtraData) const
{
	bool result = m_cheatsNotified.IsSet(cheat);

	if (result)
	{
		//Exceptions 
		if (
			(cheat == NetworkRemoteCheaterDetector::CODE_TAMPERING 
			|| cheat == NetworkRemoteCheaterDetector::CRC_CODE_CRCS)
			&& cheatExtraData > 0)
		{
			result = false;

			for (u32 i=0; i<m_cheatsTamperCodes.GetCount() && !result; i++)
			{
				if (cheatExtraData == m_cheatsTamperCodes[i])
				{
					result = true;
					break;
				}
			}
		}
	}

	return  result;
}

void CNetGamePlayer::SetCheatAsNotified(const u32 cheat, const u32 cheatExtraData)
{
	m_cheatsNotified.Set(cheat);

	//Exceptions to the rule
	if (
		(cheat == NetworkRemoteCheaterDetector::CODE_TAMPERING 
		|| cheat == NetworkRemoteCheaterDetector::CRC_CODE_CRCS)
		&& cheatExtraData > 0)
	{
		for (u32 i=0; i<m_cheatsTamperCodes.GetCount(); i++)
		{
			if (cheatExtraData == m_cheatsTamperCodes[i])
			{
				return;
			}
		}

		if (m_cheatsTamperCodes.IsFull())
			m_cheatsTamperCodes.Pop();

		m_cheatsTamperCodes.Push(cheatExtraData);
	}
}

void CNetGamePlayer::SetMuteData( u32 muteCount, u32 talkerCount )
{
	bool bDirty = false;
	if(IsLocal())
	{
		//If either has changed on the local and the mute count is positive, dirty up the node.
		bDirty = m_MuteBeenMutedCount != muteCount || m_MuteTalkersMetCount != talkerCount || m_MuteBeenMutedCount > 0;

#if __BANK
		if( m_MuteBeenMutedCount != muteCount || m_MuteTalkersMetCount != talkerCount )
		{
			gnetDebug3("SetMuteData: updating local muteCount = %u -> %u   talkerCount = %u -> %u", m_MuteBeenMutedCount, muteCount, m_MuteTalkersMetCount, talkerCount);
		}
#endif
	}
	
	m_MuteBeenMutedCount = muteCount;
	m_MuteTalkersMetCount = talkerCount;
	
	if(bDirty && IsLocal())
	{
		DirtyGamerDataSyncNode();
	}
}

void CNetGamePlayer::SetClanMembershipInfo( const rlClanMembershipData& clanMemberInfo )
{
	//remember the old one.
	rlClanMembershipData oldClanMemInfo = m_clanMembershipInfo;
    rlClanId oldClanId = m_nCrewId;

	if(m_nCrewId != oldClanId && m_PlayerInfo && gnetVerify(GetPlayerPed()))
	{
		GetPlayerPed()->SetSavedClanId(m_nCrewId);
		gnetDebug1("Setting saved ClanId for Ped ped=0x%p to %d", (void*)GetPlayerPed(),  (s32)m_nCrewId);
	}
	
	//Update the clan Desc
	m_clanMembershipInfo = clanMemberInfo;
    m_nCrewId = m_clanMembershipInfo.m_Clan.m_Id;

	//If it's the local player, tell everyone else by triggering an update of the sync node.
	if (m_clanMembershipInfo.m_Id != oldClanMemInfo.m_Id
		|| oldClanMemInfo.m_Clan.m_Id != m_clanMembershipInfo.m_Clan.m_Id
		|| oldClanMemInfo.m_Rank.m_RankOrder != m_clanMembershipInfo.m_Rank.m_RankOrder 
		)
	{
		if(IsLocal())
		{
			DirtyGamerDataSyncNode();
		}
	}

	//release reference to the old one.
	if (oldClanMemInfo.m_Clan.m_Id != m_clanMembershipInfo.m_Clan.m_Id)
	{
		if (oldClanMemInfo.IsValid() && m_bEmblemRefd)
		{
			CLiveManager::GetNetworkClan().ReleaseEmblemReference(oldClanMemInfo.m_Clan.m_Id  ASSERT_ONLY(, "CNetGamePlayer"));
			m_bEmblemRefd=false;
		}
		//Request the new clan ID
		if (m_clanMembershipInfo.IsValid())
		{
			if(CLiveManager::GetNetworkClan().RequestEmblemForClan(m_clanMembershipInfo.m_Clan.m_Id  ASSERT_ONLY(, "CNetGamePlayer")))
			{
				m_bEmblemRefd=true;
			}
        }
	}

    // need to refresh the session crew tracking
    if(oldClanId != m_nCrewId)
        CNetwork::GetNetworkSession().UpdateCurrentUniqueCrews(SessionType::ST_Physical);
}

void CNetGamePlayer::SetCrewRankTitle( const char* crewRankTitle )
{
	bool bStringsDiff = atStringHash(m_crewRankTitle) != atStringHash(crewRankTitle);

	if(!bStringsDiff)
		return;

	safecpy(m_crewRankTitle, crewRankTitle); 

	if(bStringsDiff && IsLocal())
	{
		DirtyGamerDataSyncNode();
	}
}

bool CNetGamePlayer::GetClanEmblemTXDName( char* outStr, int maxStrSize ) const
{
	if (m_clanMembershipInfo.IsValid())
	{
		const char* emblemName = CLiveManager::GetNetworkClan().GetClanEmblemNameForClan(m_clanMembershipInfo.m_Clan.m_Id);
		if (emblemName && strlen(emblemName) > 0)
		{
			safecpy(outStr, emblemName, maxStrSize);
			return true;
		}
	}

	return false;
}

void CNetGamePlayer::SetMatchmakingGroup(eMatchmakingGroup nGroup, bool bUpdateGroup)
{
    // need to refresh the session group tracking
    if(nGroup != m_MatchmakingGroup)
    {
        m_MatchmakingGroup = nGroup; 
        if(bUpdateGroup)
            CNetwork::GetNetworkSession().UpdateMatchmakingGroups();
    }
}

void CNetGamePlayer::DirtyGamerDataSyncNode()
{
	// we cannot call this on remote players
	if(!gnetVerify(IsLocal()))
		return;

	bool bDirtySet = false;

	if(IsActive() && GetPlayerInfo())
	{
		CPed* pPlayerPed = GetPlayerPed();
		if(pPlayerPed && pPlayerPed->GetNetworkObject() && pPlayerPed->GetNetworkObject()->GetSyncData())
		{
			CPlayerSyncTree* pSyncTree = static_cast<CPlayerSyncTree*>(pPlayerPed->GetNetworkObject()->GetSyncTree());
		
			pSyncTree->DirtyNode(pPlayerPed->GetNetworkObject(), *pSyncTree->GetPlayerGamerDataNode());
			bDirtySet = true; 

			// reset tracking flag
			m_bGamerDataDirtyPending = false;
		}
	}

	if(!bDirtySet)
		m_bGamerDataDirtyPending = true; 
}

const rlGamerInfo& CNetGamePlayer::GetGamerInfo() const
{
//  gnetAssert(this->GetPlayerPed());
//    return this->GetPlayerPed()->GetPlayerInfo()->m_GamerInfo;
    gnetAssert(m_PlayerInfo);
	//@@: location CNETGAMEPLAYER_GETGAMERINFO
    return m_PlayerInfo->m_GamerInfo;
}

void CNetGamePlayer::SetGamerInfo(const rlGamerInfo &gamerInfo)
{
    gnetAssert(!m_PlayerInfo);

	m_PlayerInfo = rage_new CPlayerInfo(NULL, &gamerInfo);

	gnetDebug2("CNetGamePlayer::SetGamerInfo - Creating new player info (%p). name %s", m_PlayerInfo, gamerInfo.GetName());
#if !__NO_OUTPUT
	if ((Channel_net.FileLevel >= DIAG_SEVERITY_DEBUG2) || (Channel_net.TtyLevel >= DIAG_SEVERITY_DEBUG2))
	{
		sysStack::PrintStackTrace();
	}
#endif

    gnetAssert(m_PlayerInfo);
}

bool CNetGamePlayer::IsHost() const
{
    if (GetPlayerType() != NETWORK_BOT)
    {
		u64 peerId;
		CNetwork::GetNetworkSession().GetSnSession().GetHostPeerId(&peerId);
		return peerId == GetPeerInfo().GetPeerId();
    }

    return false;
}

bool CNetGamePlayer::IsPhysical() const
{
	return this->GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX;
}

const char* CNetGamePlayer::GetLogName() const
{
    if (m_PlayerInfo)
        return GetGamerInfo().GetName();

    return "";
}

CPed* CNetGamePlayer::GetPlayerPed() const
{
    gnetAssert(IsValid());
    gnetAssert(m_PlayerInfo);
    return m_PlayerInfo ? m_PlayerInfo->GetPlayerPed() : NULL;
}

CPlayerInfo* CNetGamePlayer::GetPlayerInfo() const
{
    gnetAssert(IsValid());
    return m_PlayerInfo;
}

void CNetGamePlayer::SetPlayerInfo(CPlayerInfo *playerInfo)
{
    gnetAssert(!m_PlayerInfo);
    gnetAssert(playerInfo);

    m_PlayerInfo = playerInfo;

	// we need to reset some state here when transitioning from SP
	if (IsMyPlayer())
	{
		playerInfo->MultiplayerReset();

		// copy matchmaking group and spectator setting from network session
		SetMatchmakingGroup(CNetwork::GetNetworkSession().GetMatchmakingGroup());
        SetSpectator(CNetwork::GetNetworkSession().IsActivitySpectator());
        
#if RSG_PC
		u8 pedDensity = static_cast<u8>(CSettingsManager::GetInstance().GetSettings().m_graphics.m_CityDensity*10.0f);
		SetPedDensity(pedDensity);
#endif

		// grab the local players primary clan
		// update the local real players clan info when it changes
		int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		const rlClanMembershipData& primaryClanMembership = rlClan::GetPrimaryMembership(localGamerIndex);
		if (primaryClanMembership.IsValid())
		{
			SetClanMembershipInfo(primaryClanMembership);
            SetCrewId(primaryClanMembership.m_Clan.m_Id);

			if(GetPlayerPed())
			{
				GetPlayerPed()->SetSavedClanId(primaryClanMembership.m_Clan.m_Id);
			}
		}
	
		//Update the mute count
		SetMuteData(StatsInterface::GetUInt32Stat(STAT_PLAYER_MUTED), StatsInterface::GetUInt32Stat(STAT_PLAYER_MUTED_TALKERS_MET));
	}

	// copy team from player info
	SetTeam(playerInfo->Team);

    //update the is local flag based on the player info
    SetIsLocal(GetGamerInfo().IsLocal());
}

bool CNetGamePlayer::IsInRoamingBubble() const
{
	return m_RoamingBubbleMemberInfo.IsValid();
}

bool CNetGamePlayer::IsInSameRoamingBubble(const CNetGamePlayer& player) const
{
	return (IsInRoamingBubble() && player.IsInRoamingBubble() && player.GetRoamingBubbleMemberInfo().GetBubbleID() == m_RoamingBubbleMemberInfo.GetBubbleID());
}

CRoamingBubbleMemberInfo& CNetGamePlayer::GetRoamingBubbleMemberInfo()
{
	return m_RoamingBubbleMemberInfo;
}

const CRoamingBubbleMemberInfo& CNetGamePlayer::GetRoamingBubbleMemberInfo() const
{
	return m_RoamingBubbleMemberInfo;
}

void CNetGamePlayer::SetRoamingBubbleMemberInfo(const CRoamingBubbleMemberInfo& info)
{
	m_RoamingBubbleMemberInfo.Copy(info);

	if(GetNonPhysicalData())
	{
		static_cast<CNonPhysicalPlayerData *>(GetNonPhysicalData())->SetRoamingBubbleMemberInfo(info);
	}
}

bool CNetGamePlayer::CanAcceptMigratingObjects() const
{
	if (gnetVerify(m_PlayerInfo))
	{
        if(m_PlayerInfo->GetPlayerState() == CPlayerInfo::PLAYERSTATE_HASDIED && NetworkInterface::IsASpectatingPlayer(GetPlayerPed()))
        {
            return true;
        }
        else
        {
		    return (m_PlayerInfo->GetPlayerState() != CPlayerInfo::PLAYERSTATE_HASDIED && 
				    m_PlayerInfo->GetPlayerState() != CPlayerInfo::PLAYERSTATE_RESPAWN && 
				    m_PlayerInfo->GetPlayerState() != CPlayerInfo::PLAYERSTATE_IN_MP_CUTSCENE);
        }
	}

	return false;
}

void CNetGamePlayer::SetLocalTextChatFlag(const TextChatFlag flag, const bool value)
{
	gnetAssert(0 == (TEXT_REMOTE_MASK & flag));

	const unsigned oldFlags = this->GetLocalTextChatFlags();

	unsigned flags = m_TextChatFlags & TEXT_LOCAL_MASK;
	flags = value ? (flags | flag) : (flags & ~flag);

	m_TextChatFlags = this->GetRemoteTextChatFlags() | (flags & TEXT_LOCAL_MASK);

	m_TextChatFlagsDirty =
		m_TextChatFlagsDirty || (this->GetLocalTextChatFlags() != oldFlags);
}

void CNetGamePlayer::SetRemoteTextChatFlags(const unsigned flags)
{
	gnetAssert(0 == (TEXT_LOCAL_MASK & flags));

	m_TextChatFlags = this->GetLocalTextChatFlags() | (flags & TEXT_REMOTE_MASK);
}

unsigned CNetGamePlayer::GetLocalTextChatFlags() const
{
	return m_TextChatFlags & TEXT_LOCAL_MASK;
}

unsigned CNetGamePlayer::GetRemoteTextChatFlags() const
{
	return m_TextChatFlags & TEXT_REMOTE_MASK;
}

unsigned CNetGamePlayer::AreTextChatFlagsDirty() const
{
	return m_TextChatFlagsDirty;
}

void CNetGamePlayer::CleanDirtyTextChatFlags()
{
	m_TextChatFlagsDirty = false;
}

unsigned CNetGamePlayer::LocalToRemoteTextChatFlags(const unsigned localFlags)
{
	gnetAssert(!(localFlags & TEXT_REMOTE_MASK));

	return (localFlags << TEXT_LOCAL_TO_REMOTE_SHIFT) & TEXT_REMOTE_MASK;
}

bool CNetGamePlayer::HasKeyboard() const
{
	return this->IsLocal()
		? (0 != (TEXT_LOCAL_HAS_KEYBOARD & m_TextChatFlags))
		: (0 != (TEXT_REMOTE_HAS_KEYBOARD & m_TextChatFlags));
}

unsigned CNetGamePlayer::RemoteToLocalTextChatFlags(const unsigned remoteFlags)
{
	gnetAssert(!(remoteFlags & TEXT_LOCAL_MASK));

	return (remoteFlags >> TEXT_LOCAL_TO_REMOTE_SHIFT) & TEXT_LOCAL_MASK;
}

#if RSG_DURANGO
void CNetGamePlayer::SetDisplayName(rlDisplayName* name)
{
	m_PlayerInfo->m_GamerInfo.SetDisplayName(name);
}
#endif // RSG_XBOX





