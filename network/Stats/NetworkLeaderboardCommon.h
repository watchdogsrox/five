//
// NetworkLeaderboardCommon.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef _NETWORK_LEADERBOARDS_COMMON_H
#define _NETWORK_LEADERBOARDS_COMMON_H


//Gamer's Ranks
#define MP_RANK_0  (0)
#define MP_RANK_1  (1000)
#define MP_RANK_2  (10000)
#define MP_RANK_3  (50000)
#define MP_RANK_4  (100000)
#define MP_RANK_5  (250000)
#define MP_RANK_6  (500000)
#define MP_RANK_7  (750000)
#define MP_RANK_8  (1000000)
#define MP_RANK_9  (2500000)
#define MP_RANK_10 (5000000)

//Gamer Ranks
enum eGamersRanks
{
	MP_GAMERRANK_NO_RANK = -1,
	MP_GAMERRANK_0 = 0,
	MP_GAMERRANK_1,
	MP_GAMERRANK_2,
	MP_GAMERRANK_3,
	MP_GAMERRANK_4,
	MP_GAMERRANK_5,
	MP_GAMERRANK_6,
	MP_GAMERRANK_7,
	MP_GAMERRANK_8,
	MP_GAMERRANK_9,
	MP_GAMERRANK_10,

	MAX_MP_GAMERRANK
};

#endif // _NETWORK_LEADERBOARDS_COMMON_H

// EOF
