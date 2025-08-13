//
// netplayermessages.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#include "NetGamePlayerMessages.h"
#include "network/Sessions/NetworkGameConfig.h"
#include "network/General/NetworkUtil.h"

// framework includes
#include "fwnet/netinterface.h"
#include "fwnet/netlogsplitter.h"
#include "fwnet/netplayermgrbase.h"

NETWORK_OPTIMISATIONS()

CNetGamePlayerDataMsg::CNetGamePlayerDataMsg()
: m_PlayerType(CNetGamePlayer::NETWORK_PLAYER)
, m_MatchmakingGroup(MM_GROUP_INVALID)
, m_PlayerFlags(0)
, m_Team(-1)
, m_AimPreference(CPedTargetEvaluator::TARGETING_OPTION_GTA_TRADITIONAL)
, m_ClanId(RL_INVALID_CLAN_ID)
, m_CharacterRank(0)
, m_ELO(0)
, m_Region(0)
{
}

CNetGamePlayerDataMsg::~CNetGamePlayerDataMsg()
{
}

void CNetGamePlayerDataMsg::Reset(const unsigned gameVersion,
								  const netNatType natType,
                                  const CNetGamePlayer::PlayerType playerType,
								  const eMatchmakingGroup mmGroup,
                                  const unsigned nPlayerFlags,
								  const int team,
								  const s32 nAimPreference,
								  const rlClanId nClanId,
                                  const u16 nCharacterRank,
								  const u16 nELO,
								  const unsigned nRegion)
{
    playerDataMsg::Reset(gameVersion, natType);

    m_PlayerType = playerType;
	m_MatchmakingGroup = mmGroup;
    m_PlayerFlags = nPlayerFlags;
	m_Team = team;
	m_AimPreference	= nAimPreference;
	m_ClanId = nClanId;
    m_CharacterRank = nCharacterRank;
    m_ELO = nELO;
	m_Region = nRegion;
}

int CNetGamePlayerDataMsg::GetMessageDataBitSize() const
{
    int dataSize = playerDataMsg::GetMessageDataBitSize();

    dataSize += SIZEOF_PLAYER_TYPE;
	dataSize += SIZEOF_MM_GROUP;
    dataSize += SIZEOF_PLAYER_FLAGS;
	dataSize += SIZEOF_TEAM;
    dataSize += SIZEOF_AIM_PREFERENCE;
	dataSize += SIZEOF_CLAN_ID;
    dataSize += SIZEOF_CHARACTER_RANK;
	dataSize += SIZEOF_ELO; 
	dataSize += SIZEOF_REGION;

    return dataSize;
}

void CNetGamePlayerDataMsg::WriteToLogFile( bool bReceived, netSequence seqNum, const netPlayer &player ) const
{
    playerDataMsg::WriteToLogFile(bReceived, seqNum, player);

#if !__NO_OUTPUT
    netLogSplitter log(netInterface::GetMessageLog(), netInterface::GetPlayerMgr().GetLog());

    const char *playerTypeName = "Unknown";

    switch(m_PlayerType)
    {
    case CNetGamePlayer::NETWORK_PLAYER:
        playerTypeName = "Player";
        break;
    case CNetGamePlayer::NETWORK_BOT:
        playerTypeName = "Network Bot";
        break;
	default:
        break;
    };

	const char *mmGroupName = "Unknown";

	switch(m_MatchmakingGroup)
	{
	case MM_GROUP_FREEMODER:
		mmGroupName = "Freemoder";
		break;
	case MM_GROUP_COP:
		mmGroupName = "Cop";
		break;
	case MM_GROUP_VAGOS:
		mmGroupName = "Vagos";
		break;
	case MM_GROUP_LOST:
		mmGroupName = "Lost";
		break;
	case MM_GROUP_SCTV:
		mmGroupName = "SCTV";
		break;
	default:
		break;
	};

    log.WriteDataValue("Player Type", "%s", playerTypeName);
	log.WriteDataValue("Matchmaking Group", "%s", mmGroupName);
    log.WriteDataValue("Player Flags", "0x%x", m_PlayerFlags);
    log.WriteDataValue("Team", "%d", m_Team);
	log.WriteDataValue("Aim Preference", "%d", m_AimPreference);
    log.WriteDataValue("ClanId", "0x%" I64FMT "x", m_ClanId);
    log.WriteDataValue("Character Rank", "%d", m_CharacterRank);
    log.WriteDataValue("ELO", "%d", m_ELO);
	log.WriteDataValue("Region", "%u", m_Region);
#endif
}

bool CNetGamePlayerDataMsg::SerialiseAdditionalPlayerData(datImportBuffer &bb)
{
    return bb.SerUns(m_PlayerType, SIZEOF_PLAYER_TYPE)
		   && bb.SerUns(m_MatchmakingGroup, SIZEOF_MM_GROUP)
           && bb.SerUns(m_PlayerFlags, SIZEOF_PLAYER_FLAGS)
           && bb.SerInt(m_Team, SIZEOF_TEAM)
           && bb.SerUns(m_AimPreference, SIZEOF_AIM_PREFERENCE)
		   && bb.SerInt(m_ClanId, SIZEOF_CLAN_ID)
           && bb.SerUns(m_CharacterRank, SIZEOF_CHARACTER_RANK)
		   && bb.SerUns(m_ELO, SIZEOF_ELO)
		   && bb.SerUns(m_Region, SIZEOF_REGION);
}

bool CNetGamePlayerDataMsg::SerialiseAdditionalPlayerData(datExportBuffer &bb) const
{
    return bb.SerUns(m_PlayerType, SIZEOF_PLAYER_TYPE)
		   && bb.SerUns(m_MatchmakingGroup, SIZEOF_MM_GROUP)
		   && bb.SerUns(m_PlayerFlags, SIZEOF_PLAYER_FLAGS)
           && bb.SerInt(m_Team, SIZEOF_TEAM)
		   && bb.SerUns(m_AimPreference, SIZEOF_AIM_PREFERENCE)
		   && bb.SerInt(m_ClanId, SIZEOF_CLAN_ID)
           && bb.SerUns(m_CharacterRank, SIZEOF_CHARACTER_RANK)
           && bb.SerUns(m_ELO, SIZEOF_ELO)
		   && bb.SerUns(m_Region, SIZEOF_REGION);
}
