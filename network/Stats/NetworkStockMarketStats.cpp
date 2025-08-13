//
// NetworkCommunityStats.cpp
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//


//Rage Includes
#include "parser/macros.h"

//framework Headers

//Game Headers
#include "NetworkStockMarketStats.h"
#include "stats/StatsMgr.h"
#include "stats/StatsDataMgr.h"
#include "scene/DataFileMgr.h"
#include "system/fileMgr.h"
#include "network/Stats/CommunityStatsData_parser.h"
#include "stats/stats_channel.h"

void
CNetworkCommunityStats::Init()
{
	RegisterMetadatafile();
	LoadCommunityStatsData();
}

class CNetworkCommunityStatsFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile& file)
	{
		switch (file.m_fileType)
		{
		case CDataFileMgr::COMMUNITY_STATS_FILE:
			CStatsMgr::GetStatsDataMgr().GetCommunityStatsMgr().LoadCommunityStatsData();
		default:
			break;
		}
		return true;
	}
	virtual void UnloadDataFile(const CDataFileMgr::DataFile & /*file*/)
	{

	}
}g_CommunityStatsFileMounter;

void CNetworkCommunityStats::RegisterMetadatafile()
{
	CDataFileMount::RegisterMountInterface(CDataFileMgr::COMMUNITY_STATS_FILE, &g_CommunityStatsFileMounter);
}

void CNetworkCommunityStats::LoadCommunityStatsData()
{
	strStreamingEngine::GetLoader().CallKeepAliveCallback();

	// Iterate through the files backwards, so episodic data can override any old data
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetLastFile(CDataFileMgr::COMMUNITY_STATS_FILE);

	while (DATAFILEMGR.IsValid(pData))
	{
		if (!pData->m_disabled)
		{
			LoadCommunityStatsMetaData(pData->m_filename);
		}
		pData = DATAFILEMGR.GetPrevFile(pData);
		strStreamingEngine::GetLoader().CallKeepAliveCallbackIfNecessary();
	}
}

void CNetworkCommunityStats::LoadCommunityStatsMetaData(const char* pFile)
{
	m_StatsMetadata.m_Data.clear();

	FileHandle fid = CFileMgr::OpenFile(pFile);
	statAssertf(CFileMgr::IsValidFileHandle(fid), "%s:Cannot find community stats file.", pFile);

	PARSER.LoadObject((fiStream*)fid, m_StatsMetadata);
	CFileMgr::CloseFile(fid);
}

bool CNetworkCommunityStats::GetCommunityStatDepthValue(const int statId, float* returnValue, const unsigned depth)
{
	bool result = false;

	*returnValue = 0.0f;

	for (const CCommunityStatsData& stat : m_StatsMetadata.m_Data)
	{
		if ((u32)statId == stat.m_StatId.GetHash())
		{
			if (gnetVerify(depth < stat.m_HistoryDepth.GetCount()))
			{
				result = true;
				*returnValue = stat.m_HistoryDepth[depth];
			}

			break;
		}
	}

	return result;
}

//eof

