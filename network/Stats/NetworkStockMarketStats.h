//
// NetworkCommunityStats.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef _NETWORK_COMMUNITY_STATS_H
#define _NETWORK_COMMUNITY_STATS_H

//Rage Includes
#include "atl/array.h"
#include "atl/hashstring.h"
#include "data/base.h"
#include "parser/macros.h"

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/// class CCommunityStatsData
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////


class CCommunityStatsData : public ::rage::datBase
{
public:
	CCommunityStatsData() {};                // Default constructor
	virtual ~CCommunityStatsData() {};        // Virtual destructor

	::rage::atHashWithStringNotFinal                m_StatId;
	float                                           m_Value;
	::rage::atArray< float, 0, ::rage::u16>         m_HistoryDepth;

	PAR_PARSABLE;
};


/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
/// class CCommunityStatsDataMgr
/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////


class CCommunityStatsDataMgr : public ::rage::datBase
{
public:
	CCommunityStatsDataMgr() {};                 // Default constructor
	virtual ~CCommunityStatsDataMgr() {};        // Virtual destructor

	::rage::atArray< CCommunityStatsData, 0, ::rage::u16>         m_Data;

	PAR_PARSABLE;
};

//PURPOSE
//  Class responsible to manage Community stats.
class CNetworkCommunityStats
{
private:
	//Community stats metadata file.
	CCommunityStatsDataMgr m_StatsMetadata;

public:
	void Init();

	void LoadCommunityStatsData();
	bool GetCommunityStatDepthValue(const int statId, float* returnValue, const unsigned depth);

	const CCommunityStatsDataMgr& GetMetadata() const { return m_StatsMetadata; }

private:
	void LoadCommunityStatsMetaData(const char* pFile);
	void RegisterMetadatafile();
};

#endif // _NETWORK_COMMUNITY_STATS_H

// EOF
