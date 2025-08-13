//
// NetGamePlayerMessages.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef NETGAMEPLAYERMESSAGES_H
#define NETGAMEPLAYERMESSAGES_H

// framework headers
#include "fwnet/netplayermessages.h"

// game headers
#include "network/NetworkTypes.h"
#include "network/general/NetworkAssetVerifier.h"
#include "network/general/NetworkColours.h"
#include "network/players/NetGamePlayer.h"
#include "Peds/PedWeapons/PedTargetEvaluator.h"

//PURPOSE
// A message containing data about a network player when joining a session
class CNetGamePlayerDataMsg : public playerDataMsg
{
public:

    CNetGamePlayerDataMsg();
    virtual ~CNetGamePlayerDataMsg();

	enum 
	{
		PLAYER_MSG_FLAG_SPECTATOR		= BIT0,
		PLAYER_MSG_FLAG_ROCKSTAR_DEV	= BIT1,
		PLAYER_MSG_FLAG_BOSS			= BIT2,
		PLAYER_MSG_FLAG_IS_ASYNC		= BIT3,
		PLAYER_MSG_FLAG_NUM				= 4,
	};

    //PURPOSE
    // Resets the contents of this message
	void Reset(const unsigned gameVersion,
			   const netNatType natType,
               const CNetGamePlayer::PlayerType playerType,
			   const eMatchmakingGroup mmGroup,
               const unsigned nPlayerFlags,
               const int team,
			   const s32 nAimPreference,
			   const rlClanId nClanId,
               const u16 nCharacterRank,
			   const u16 nELO,
			   const unsigned nRegion);

    //PURPOSE
    // Returns the size of the message data in bits
    virtual int GetMessageDataBitSize() const;

    //PURPOSE
    // Writes the contents of this message to a log file
    //PARAMS
    // wasReceived - Indicates whether this message is being sent or has been received
    // sequenceNum - The sequence number used for sending/receiving the message
    // player      - The player to send/receive the message
    virtual void WriteToLogFile(bool bReceived, netSequence seqNum, const netPlayer &player) const;

    static const unsigned SIZEOF_PLAYER_TYPE		= datBitsNeeded<CNetGamePlayer::MAX_PLAYER_TYPES>::COUNT;
	static const unsigned SIZEOF_MM_GROUP			= datBitsNeeded<MM_GROUP_MAX>::COUNT;
	static const unsigned SIZEOF_PLAYER_FLAGS		= PLAYER_MSG_FLAG_NUM;
    static const unsigned SIZEOF_TEAM				= datBitsNeeded<MAX_NUM_TEAMS>::COUNT + 1;
	static const unsigned SIZEOF_AIM_PREFERENCE		= datBitsNeeded<CPedTargetEvaluator::MAX_TARGETING_OPTIONS>::COUNT;
	static const unsigned SIZEOF_CLAN_ID			= 64;
    static const unsigned SIZEOF_CHARACTER_RANK     = 16;
    static const unsigned SIZEOF_ELO                = 16;
	static const unsigned SIZEOF_REGION				= 8;

    virtual bool SerialiseAdditionalPlayerData(datImportBuffer &bb);
    virtual bool SerialiseAdditionalPlayerData(datExportBuffer &bb) const;

	// used to know which player type to add
	CNetGamePlayer::PlayerType m_PlayerType;

	// this data is all required by SetCustomPlayerData
	eMatchmakingGroup m_MatchmakingGroup;
	unsigned m_PlayerFlags;
	int m_Team;
	rlClanId m_ClanId;

	s32 m_AimPreference;	// needed in player data to capture the host aim preference
    u16 m_CharacterRank;	// used for transition / competitive only
    u16 m_ELO;				// used for transition / competitive only
	unsigned m_Region;		// used for logging only
};

#endif  // NETGAMEPLAYERMESSAGES_H
