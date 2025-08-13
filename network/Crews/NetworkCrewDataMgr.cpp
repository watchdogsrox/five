#include "NetworkCrewDataMgr.h"

#include "Network/Stats/NetworkLeaderboardSessionMgr.h"
#include "Network/live/NetworkClan.h"
#include "frontend/ui_channel.h"
#include "fwnet/netscgameconfigparser.h"
#include "fwsys/gameskeleton.h"
#include "game/config.h"
#include "network/NetworkInterface.h"
#include "network/live/livemanager.h"
#include "optimisations.h"
#include "rline/clan/rlclan.h"
#include "rline/ros/rlros.h"

#if __BANK
#include "bank/bank.h"
#endif

BankInt32 FAIL_CLAN_TIMEOUT = 60000;

NETWORK_OPTIMISATIONS()
FRONTEND_NETWORK_OPTIMISATIONS()

#define NUM_STRIKES 3
#define STRIKES_MASK 0x3
#define STRIKES_BITSIZE 2
#define TOTAL_RETRIES 8 // works out to be 
const u8 MAX_IN_FLIGHT_QUERIES = 4;

#define HAS_STRUCK_OUT(counter) (counter)==NUM_STRIKES

const Leaderboard*	CNetworkCrewData::sm_pLBXPData = NULL;
#define LEADERBOARD_FOR_XP "FREEMODE_PLAYER_XP"


//////////////////////////////////////////////////////////////////////////
IMPLEMENT_CLASS_NEW_DELETE(CNetworkCrewData, "CNetworkCrewData");

void CNetworkCrewData::InitPool(int iSize)
{
	if( gnetVerifyf(GetCurrentPool() == NULL, "Have already initialized the CNetworkCrewData pool!" ) )
	{
		SetCurrentPool( rage_new atPool<CNetworkCrewData>(iSize) );
	}
}

void CNetworkCrewData::InitPool(int iSize, float spillover)
{
	if( gnetVerifyf(GetCurrentPool() == NULL, "Have already initialized the CNetworkCrewData pool!" ) )
	{
		SetCurrentPool( rage_new atPool<CNetworkCrewData>(iSize, spillover) );
	}
}

void CNetworkCrewData::ShutdownPool()
{
	if( gnetVerifyf(GetCurrentPool() != NULL, "Shutting down the CNetworkCrewData pool without ever having set it up!" ) )
	{
		delete sm_CurrentPool; 
		sm_CurrentPool = NULL;
	}
}

//////////////////////////////////////////////////////////////////////////

CNetworkCrewData::CNetworkCrewData()
	: m_RequestedClan( RL_INVALID_CLAN_ID )
	, m_AllFailures(0)
	, m_AttemptsCount(0)
	, m_bSuccess(false)
	, m_bEmblemRefd(false)
	, m_requestLeaderboard(true)
{

}

CNetworkCrewData::~CNetworkCrewData()
{
	Shutdown();
}


//////////////////////////////////////////////////////////////////////////

bool CNetworkCrewData::Init( const rlClanId newId, bool requestLeaderboard /* = true*/)
{
	Shutdown();

	if( !gnetVerifyf( newId!=RL_INVALID_CLAN_ID, "Can't initialize to an invalid clan id!" ) )
		return false;

	const rlGamerInfo* pInfo = NetworkInterface::GetActiveGamerInfo();
	if( !gnetVerifyf(pInfo, "No active player!?" ))
		return false;

	if( !sm_pLBXPData )
		sm_pLBXPData = GAME_CONFIG_PARSER.GetLeaderboard(LEADERBOARD_FOR_XP);

	m_requestLeaderboard = requestLeaderboard;
	
	const rlGamerHandle& forThisPlayer = pInfo->GetGamerHandle();
	const int localGamerIndex = forThisPlayer.GetLocalIndex();
	if (RL_VERIFY_LOCAL_GAMER_INDEX(localGamerIndex))
	{
		const rlRosCredentials& creds = rlRos::GetCredentials(localGamerIndex);
		if (gnetVerifyf(creds.IsValid(), "Invalid ROS credentials for index %i", localGamerIndex))
		{
			if (gnetVerifyf(rlClan::GetDesc(localGamerIndex, newId, &m_ClanInfo[0], &m_ClanInfo.m_RequestStatus), "Unable to get the clan description of Clan %" I64FMT "i", newId))
			{
				if (m_requestLeaderboard || !CLiveManager::GetCrewDataMgr().GetWithSeparateLeaderboardRequests())
				{
					requestLeaderboardData();
				}

				uiDebugf1("Requested Crew Data for %" I64FMT "i", newId);

				m_Metadata.SetClanId(newId);
				m_RequestedClan = newId;

				NetworkClan& clanMgr = CLiveManager::GetNetworkClan();
				if (clanMgr.RequestEmblemForClan(newId  ASSERT_ONLY(, "CNetworkCrewData")))
					m_bEmblemRefd = true;
				return true;
			}
		}
	}

	return false;
}

void CNetworkCrewData::Shutdown()
{
	m_Metadata.Clear();
	m_ClanInfo.Clear();

	ReleaseLB(m_LBXPRead);
	
	if( m_RequestedClan != RL_INVALID_CLAN_ID && m_bEmblemRefd)
	{
		NetworkClan& clanMgr = CLiveManager::GetNetworkClan();
		clanMgr.ReleaseEmblemReference(m_RequestedClan  ASSERT_ONLY(, "CNetworkCrewData"));
	}
	else
		uiAssertf(!m_bEmblemRefd, "Had a CrewData that apparently ref'd a clan emblem but can't remember WHICH clan it was!");

	m_RequestedClan = RL_INVALID_CLAN_ID;
	m_AllFailures = 0;
	m_FailTimer.Zero();
	m_bSuccess = false;
	m_bEmblemRefd = false;
	m_requestLeaderboard = true;
}

netStatus::StatusCode CNetworkCrewData::StartFailTimer(const char* OUTPUT_ONLY(Reason) )
{
	if( !m_FailTimer.IsStarted() )
	{
		uiErrorf("Clan Data for %" I64FMT "d has struck out on its %s read!", m_RequestedClan, Reason);
		m_FailTimer.Start();
		++m_AttemptsCount; // pump that up!
	}

	return  m_AttemptsCount <= TOTAL_RETRIES ? netStatus::NET_STATUS_PENDING : netStatus::NET_STATUS_FAILED; // stay alive as long as we haven't overstayed our welcome
}

netStatus::StatusCode CNetworkCrewData::Update()
{
	gnetAssert(!m_bSuccess);

	if(m_FailTimer.IsStarted() && m_FailTimer.IsComplete(FAIL_CLAN_TIMEOUT * BIT(m_AttemptsCount))) // doing a bit so we get a binary expansion
	{
		uiDisplayf("Failed Clan %" I64FMT "d has timed out from its failure, and is starting over!", m_RequestedClan);
		if( !Init(m_RequestedClan) )
			return netStatus::NET_STATUS_FAILED;
	}

	// once the LB is successful, kill it off and just keep the useful data around

	// XP READ
	if( m_LBXPRead.Get() )
	{
		if( m_LBXPRead->Failed() )
		{
			ReleaseLB(m_LBXPRead);

			if( HAS_STRUCK_OUT(m_Fails.XPRead) )
				return StartFailTimer("XP");
			
			++m_Fails.XPRead;
			uiErrorf("Clan Data for %" I64FMT "d has struck out on its XP read!", m_RequestedClan);

			const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
			if( RL_VERIFY_LOCAL_GAMER_INDEX( localGamerIndex) )
			{
				ReadLBXP(localGamerIndex, m_RequestedClan);
			}
			else
				return netStatus::NET_STATUS_PENDING;

		}
		else if( m_LBXPRead->Finished() )
		{
			if(m_LBXPRead->GetNumRows())
			{
				const rlLeaderboard2Row* pRow = m_LBXPRead->GetRow(0);
				m_LBXPSubset.WorldRank = pRow->m_Rank;
				m_LBXPSubset.WorldXP   = pRow->m_ColumnValues[0].Int64Val;
			}
			m_LBXPSubset.bReadComplete = true;
			ReleaseLB(m_LBXPRead);
		}
	}

	// DESCRIPTION
	bool bDoesntExist = (m_ClanInfo.m_RequestStatus.Failed() && m_ClanInfo.m_RequestStatus.GetResultCode() == RL_CLAN_ERR_NOT_FOUND );
	if( m_ClanInfo.m_RequestStatus.Canceled() || (m_ClanInfo.m_RequestStatus.Failed() && !bDoesntExist) )
	{
		m_ClanInfo[0].Clear();
		if( HAS_STRUCK_OUT(m_Fails.Info) )
			return StartFailTimer("Info");

		uiErrorf("Clan Data for %" I64FMT "d has struck out on its Info Read!", m_RequestedClan);
		++m_Fails.Info;

		const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		if( RL_VERIFY_LOCAL_GAMER_INDEX( localGamerIndex) )
		{
			if( !gnetVerifyf(rlClan::GetDesc(localGamerIndex, m_RequestedClan, &m_ClanInfo[0], &m_ClanInfo.m_RequestStatus), "Unable to get the clan description of Clan %" I64FMT "i", m_RequestedClan) )
				++m_Fails.Info;
		}
	}

	// METADATA

	m_Metadata.Update();
	if( m_Metadata.Failed() )
	{
		if( HAS_STRUCK_OUT(m_Fails.Metadata) )
		{
			m_Metadata.Clear();
			return StartFailTimer("Metadata");
		}
		uiErrorf("Clan Data for %" I64FMT "d has struck out on its Metadata read!", m_RequestedClan);
		++m_Fails.Metadata;

		m_Metadata.SetClanId(m_RequestedClan);
	}
	
	if(	m_LBXPSubset.bReadComplete &&
		(m_ClanInfo.m_RequestStatus.Succeeded() || bDoesntExist ) &&
		m_Metadata.Succeeded() )
	{
		m_bSuccess = true;
		return netStatus::NET_STATUS_SUCCEEDED;
	}

	return netStatus::NET_STATUS_PENDING;
}

bool CNetworkCrewData::HasFailed() const
{
	u8 fails = m_AllFailures;
	while(fails)
	{
		// if any of our values have timed out, we've failed.
		if( HAS_STRUCK_OUT(fails & STRIKES_MASK) )
			return true;
		fails >>= STRIKES_BITSIZE;
	}
	return false;
}

bool CNetworkCrewData::requestLeaderboardData()
{
	const rlGamerInfo* pInfo = NetworkInterface::GetActiveGamerInfo();
	if (!gnetVerifyf(pInfo, "No active player!?"))
		return false;

	const rlGamerHandle& forThisPlayer = pInfo->GetGamerHandle();
	const int localGamerIndex = forThisPlayer.GetLocalIndex();
	if (RL_VERIFY_LOCAL_GAMER_INDEX(localGamerIndex))
	{
		const rlRosCredentials& creds = rlRos::GetCredentials(localGamerIndex);
		if (gnetVerifyf(creds.IsValid(), "Invalid ROS credentials for index %i", localGamerIndex))
		{
			m_requestLeaderboard = true;
			if (gnetVerifyf(sm_pLBXPData, "Unable to find a leaderboard called " LEADERBOARD_FOR_XP "!"))
			{
				ReadLBXP(localGamerIndex, m_RequestedClan);
			}
			return true;
		}
	}
	return false;
}

void CNetworkCrewData::ReleaseLB(LBRead& releaseThis)
{
	if( releaseThis.Get() )
	{
		CNetworkReadLeaderboards& readmgr = CNetwork::GetLeaderboardMgr().GetLeaderboardReadMgr();
		readmgr.ClearLeaderboardRead(releaseThis);
		releaseThis = NULL;
	}
}

void CNetworkCrewData::ReadLBXP( const int localGamerIndex, const rlClanId newId )
{
	CNetworkReadLeaderboards& readmgr = CNetwork::GetLeaderboardMgr().GetLeaderboardReadMgr();
	rlLeaderboard2GroupSelector groupSelector;

	const netLeaderboardRead* pResult = readmgr.AddReadByRow(localGamerIndex
		, sm_pLBXPData->m_id
		, RL_LEADERBOARD2_TYPE_CLAN
		, groupSelector
		, RL_INVALID_CLAN_ID
		, 0, NULL
		, 1, &newId
		, 0, NULL );

	gnetAssertf(pResult, "Was unable to do a leaderboard request from " LEADERBOARD_FOR_XP);

	// need to cast it away so we can regdref it.
	m_LBXPRead = const_cast<netLeaderboardRead*>(pResult);
}

//////////////////////////////////////////////////////////////////////////

CNetworkCrewDataMgr::CNetworkCrewDataMgr()
	: m_iMaxDataQueueSize(0)
	, m_pLastFetched(NULL)
	, m_withSeparateLeaderboardRequests(false)
#if __BANK
	, m_DbgClanId(0)
	, m_bDbgFetch(false)
	, m_pDbgGroup(NULL)
	, m_pDbgData(NULL)
	, m_InFlightQueries(0)
#endif
{
}

CNetworkCrewDataMgr::~CNetworkCrewDataMgr()
{
	Shutdown(SHUTDOWN_CORE);
}

//////////////////////////////////////////////////////////////////////////

void CNetworkCrewDataMgr::Init()
{
	if( CNetworkCrewData::GetCurrentPool() == NULL )
	{
		m_iMaxDataQueueSize = fwConfigManager::GetInstance().GetSizeOfPool(ATSTRINGHASH("NetworkCrewDataMgr",0xb120a6ba), CONFIGURED_FROM_FILE);
		m_DataQueue.Reserve(m_iMaxDataQueueSize);
		CNetworkCrewData::InitPool(m_iMaxDataQueueSize, 0.75f);
	}
	m_InFlightQueries = 0;
}

//////////////////////////////////////////////////////////////////////////

void CNetworkCrewDataMgr::Shutdown(const u32 shutdownMode)
{
	if( shutdownMode == SHUTDOWN_CORE )
	{
		// This will call delete on the content objets
		m_DataQueue.Reset();

		if( CNetworkCrewData::GetCurrentPool() != NULL )
			CNetworkCrewData::ShutdownPool();
	}
}

bool CNetworkCrewDataMgr::OnSignOffline()
{
	gnetDebug1("Resetting CNetworkCrewDataMgr DataQueue (size=%d)", m_DataQueue.size() );
	gnetDebug1("Resetting CNetworkCrewDataMgr m_InFlightQueries=%d", m_InFlightQueries );

	m_InFlightQueries=0;
	for(u32 i=0; i<m_DataQueue.size(); i++)
	{
		delete m_DataQueue[i];
	}
	m_DataQueue.Reset();
	m_DataQueue.Reserve(m_iMaxDataQueueSize);
	return true;
}

//////////////////////////////////////////////////////////////////////////

void CNetworkCrewDataMgr::Update()
{
	for(int i=0; i < m_DataQueue.GetCount(); ++i)
	{
		if(!m_DataQueue[i]->m_bSuccess)
		{
			netStatus::StatusCode updateStatus = m_DataQueue[i]->Update();
			if( updateStatus == netStatus::NET_STATUS_FAILED )
			{
				Remove(i);
				--i;
				gnetAssert(m_InFlightQueries > 0);
				--m_InFlightQueries;
				uiDisplayf("CNetworkCrewDataMgr Update FAILED decrementing to %d", m_InFlightQueries);
			}
			else if( updateStatus == netStatus::NET_STATUS_SUCCEEDED )
			{
				gnetAssert(m_InFlightQueries > 0);
				--m_InFlightQueries;
				uiDisplayf("CNetworkCrewDataMgr Update SUCCESS decrementing to %d", m_InFlightQueries);
			}
		}
	}

#if __BANK
	DebugUpdate();
#endif
}

//////////////////////////////////////////////////////////////////////////

void CNetworkCrewDataMgr::Prefetch(const rlClanId clanToFetchDataFor)
{
	FindOrRequestData(clanToFetchDataFor);
}

//////////////////////////////////////////////////////////////////////////

netStatus::StatusCode CNetworkCrewDataMgr::GetClanDesc(const rlClanId clanToFetchDataFor, const rlClanDesc*&  out_Result, bool& bOUT_BecauseItsGone )
{
	bOUT_BecauseItsGone = false;
	// check if it's already been requested
	const CNetworkCrewData* const pData = FindOrRequestData(clanToFetchDataFor);
	if( !pData )
		return netStatus::NET_STATUS_NONE;

	if( pData->HasFailed() )
	{
		bOUT_BecauseItsGone = (pData->m_ClanInfo.m_RequestStatus.GetResultCode() == RL_CLAN_ERR_NOT_FOUND);
		return netStatus::NET_STATUS_FAILED;
	}
	else if( pData->m_ClanInfo.m_RequestStatus.Failed() )
	{
		bOUT_BecauseItsGone = (pData->m_ClanInfo.m_RequestStatus.GetResultCode() == RL_CLAN_ERR_NOT_FOUND);
		return netStatus::NET_STATUS_FAILED;
	}

	if( pData->m_ClanInfo.m_RequestStatus.Succeeded() && pData->m_ClanInfo[0].IsValid() )
	{
		out_Result = &pData->m_ClanInfo[0];
		return  netStatus::NET_STATUS_SUCCEEDED;
	}

	return netStatus::NET_STATUS_PENDING;
}

//////////////////////////////////////////////////////////////////////////

netStatus::StatusCode
CNetworkCrewDataMgr::IsEverythingReadyFor( const rlClanId clanToFetchDataFor, bool bAllowRequest /* = true */ )
{
	const CNetworkCrewData* const pData = FindOrRequestData(clanToFetchDataFor, bAllowRequest);
	if( !pData )
		return netStatus::NET_STATUS_NONE;

	if( pData->HasFailed() )
		return netStatus::NET_STATUS_FAILED;

	if( !pData->m_Metadata.Succeeded() )
		return netStatus::NET_STATUS_PENDING;

	if(m_withSeparateLeaderboardRequests)
	{
		if (pData->m_requestLeaderboard && !pData->m_LBXPSubset.bReadComplete)
			return netStatus::NET_STATUS_PENDING;
	}
	else
	{
		if (!pData->m_LBXPSubset.bReadComplete)
			return netStatus::NET_STATUS_PENDING;
	}

	if( !pData->m_ClanInfo.m_RequestStatus.Succeeded() || !pData->m_ClanInfo[0].IsValid() )
		return netStatus::NET_STATUS_PENDING;

	NetworkClan& clanMgr = CLiveManager::GetNetworkClan();

	if( !clanMgr.IsEmblemForClanReady(clanToFetchDataFor) )
		return netStatus::NET_STATUS_PENDING;


	return netStatus::NET_STATUS_SUCCEEDED;
}

//////////////////////////////////////////////////////////////////////////

netStatus::StatusCode CNetworkCrewDataMgr::GetClanMetadata(const rlClanId clanToFetchDataFor, const NetworkCrewMetadata*& out_Result)
{
	// check if it's already been requested
	const CNetworkCrewData* const pData = FindOrRequestData(clanToFetchDataFor, true, false);
	if( !pData )
		return netStatus::NET_STATUS_NONE;

	if( HAS_STRUCK_OUT( pData->m_Fails.Metadata ) )
		return netStatus::NET_STATUS_FAILED;

	if( pData->m_Metadata.Succeeded() )
	{
		out_Result = &pData->m_Metadata;
		return netStatus::NET_STATUS_SUCCEEDED;
	}

	return netStatus::NET_STATUS_PENDING;
}

//////////////////////////////////////////////////////////////////////////
netStatus::StatusCode CNetworkCrewDataMgr::GetClanEmblem(const rlClanId clanToFetchDataFor, const char*& out_Result, bool requestLeaderboard)
{
	const CNetworkCrewData* const pData = FindOrRequestData(clanToFetchDataFor, true, requestLeaderboard);
	if( !pData )
		return netStatus::NET_STATUS_NONE;

	// if the emblem failed on initial creation, that means we didn't ref it at all.
	// thus, we can surmise it failed for whatever reason (probably bad stuff...?)
	if( !pData->m_bEmblemRefd )
		return netStatus::NET_STATUS_FAILED;

	NetworkClan& clanMgr = CLiveManager::GetNetworkClan();
	if( clanMgr.IsEmblemForClanReady(clanToFetchDataFor, out_Result) )
		return netStatus::NET_STATUS_SUCCEEDED;

	if( clanMgr.IsEmblemForClanFailed(clanToFetchDataFor))
		return netStatus::NET_STATUS_FAILED;

	return netStatus::NET_STATUS_PENDING;
}

bool CNetworkCrewDataMgr::GetClanEmblemIsRetrying(const rlClanId clanToFetchDataFor)
{
	if( const CNetworkCrewData* const pData = FindOrRequestData(clanToFetchDataFor) )
	{
		if(pData->m_bEmblemRefd)
		{
			return CLiveManager::GetNetworkClan().IsEmblemForClanRetrying(clanToFetchDataFor);
		}
	}

	return false;
}

void CNetworkCrewDataMgr::OnTunableRead()
{
	m_withSeparateLeaderboardRequests = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SEPARATE_CREW_LEADERBOARD_REQUESTS", 0xF811969B), m_withSeparateLeaderboardRequests);
}

/* unused
bool CNetworkCrewDataMgr::GetClanRankTitle(const rlClanId clanToFetchDataFor, const int iDesiredRankInLevel, char* out_Result, int outSize)
{
	const NetworkCrewMetadata* pMetadata = NULL;
	if( GetClanMetadata(clanToFetchDataFor, pMetadata) )
	{
		if( pMetadata->GetCrewInfoCrewRankTitle(iDesiredRankInLevel,out_Result,outSize ) )
			return true;
	}

	return false;
}
*/

netStatus::StatusCode CNetworkCrewDataMgr::GetClanWorldRank(const rlClanId clanToFetchDataFor, unsigned& out_Result)
{
	const CNetworkCrewData* const pData = FindOrRequestData(clanToFetchDataFor);
	if( !pData )
		return netStatus::NET_STATUS_NONE;

	if( HAS_STRUCK_OUT(pData->m_Fails.XPRead))
		return netStatus::NET_STATUS_FAILED;

	if( pData->m_LBXPSubset.bReadComplete )
	{
		out_Result = pData->m_LBXPSubset.WorldRank;
		return netStatus::NET_STATUS_SUCCEEDED;
	}

	return netStatus::NET_STATUS_PENDING;
}

netStatus::StatusCode CNetworkCrewDataMgr::GetClanWorldXP(const rlClanId clanToFetchDataFor, s64& out_Result)
{
	const CNetworkCrewData* const pData = FindOrRequestData(clanToFetchDataFor);
	if( !pData )
		return netStatus::NET_STATUS_NONE;

	if( HAS_STRUCK_OUT(pData->m_Fails.XPRead))
		return netStatus::NET_STATUS_FAILED;

	if( pData->m_LBXPSubset.bReadComplete )
	{
		out_Result = pData->m_LBXPSubset.WorldXP;
		return netStatus::NET_STATUS_SUCCEEDED;
	}

	return netStatus::NET_STATUS_PENDING;
}

 netStatus::StatusCode CNetworkCrewDataMgr::GetClanChallengeWinLoss(const rlClanId clanToFetchDataFor, WinLossPair& out_Result)
{
	const CNetworkCrewData* const pData = FindOrRequestData(clanToFetchDataFor);
	if( !pData )
		return netStatus::NET_STATUS_NONE;

	out_Result.wins = 0;
	out_Result.losses = 0;
	return netStatus::NET_STATUS_NONE;
}

netStatus::StatusCode CNetworkCrewDataMgr::GetClanH2HWinLoss(const rlClanId clanToFetchDataFor, WinLossPair& out_Result)
{
	const CNetworkCrewData* const pData = FindOrRequestData(clanToFetchDataFor);
	if( !pData )
		return netStatus::NET_STATUS_NONE;

	out_Result.wins = 0;
	out_Result.losses = 0;
	return netStatus::NET_STATUS_NONE;
}



//////////////////////////////////////////////////////////////////////////

const CNetworkCrewData* const CNetworkCrewDataMgr::FindOrRequestData(const rlClanId findThis, bool bAllowRequest /* = true */, bool requestLeaderboard /* = true*/)
{
	CNetworkCrewData* result = nullptr;
	// check our cache first
	if (m_pLastFetched && m_pLastFetched->GetRequestedClanId() == findThis)
	{
		result = m_pLastFetched;
	}

	if(!result)
	{
		for (int i = 0; i < m_DataQueue.GetCount(); ++i)
		{
			if (m_DataQueue[i]->GetRequestedClanId() == findThis)
			{
				m_pLastFetched = m_DataQueue[i];
				result = m_pLastFetched;
			}
		}
	}

	if (result)
	{
		if (m_withSeparateLeaderboardRequests && requestLeaderboard && !result->m_requestLeaderboard)
		{
			result->requestLeaderboardData();
		}
		return result;
	}

	if(!bAllowRequest)
		return NULL;

	if(m_InFlightQueries >= MAX_IN_FLIGHT_QUERIES)
	{
		uiWarningf("CNetworkCrewDataMgr FindOrRequestData max of %d hit ignoring %" I64FMT "d", MAX_IN_FLIGHT_QUERIES, findThis);
		return NULL;
	}

	// not found, request it.
	// if full, make room
	if( m_DataQueue.GetCount() == m_DataQueue.GetCapacity() )
	{
		Remove(0);
	}

	CNetworkCrewData* pNewData = rage_new CNetworkCrewData();
	if( pNewData->Init(findThis, requestLeaderboard) )
	{
		++m_InFlightQueries;
		uiDisplayf("CNetworkCrewDataMgr FindOrRequestData new request for %" I64FMT "d %d in-flight", findThis, m_InFlightQueries);
		m_DataQueue.Push(pNewData);
		m_pLastFetched = pNewData;
		return pNewData;
	}
	delete pNewData;
	m_pLastFetched = NULL;
	return NULL;
}

void CNetworkCrewDataMgr::Remove(int index)
{
#if __BANK
	if( m_DataQueue[index] == m_pDbgData )
	{
		DebugAddBankWidgets(NULL);
	}
#endif
	delete m_DataQueue[index];
	m_DataQueue.Delete(index);
}

#if __BANK
void CNetworkCrewDataMgr::InitWidgets(bkBank& bank)
{
	bkGroup* pGroup = bank.AddGroup("Clan Data");
	{
		pGroup->AddSlider("Failure timeout", &FAIL_CLAN_TIMEOUT, 0, MAX_INT32, 1000);
		pGroup->AddSlider("Capacity", m_DataQueue.GetCountPointer(), 0, static_cast<unsigned short>(m_iMaxDataQueueSize), 0);
		pGroup->AddSeparator();
		pGroup->AddButton("Flush everything", datCallback(MFA(CNetworkCrewDataMgr::DebugFlush),this));
		pGroup->AddSeparator();
		pGroup->AddText("Clan ID to fetch", &m_DbgClanId, false, datCallback(MFA(CNetworkCrewDataMgr::DebugStartFetching),this));
		pGroup->AddButton("Fetch that ID", datCallback(MFA(CNetworkCrewDataMgr::DebugStartFetching),this));
		pGroup->AddSeparator();
		m_pDbgGroup = pGroup->AddGroup("Fetched Data", true);
	}
}

void CNetworkCrewDataMgr::DebugFlush()
{
	int i=m_DataQueue.GetCount();
	while(i--)
	{
		Remove(i);
	}
	m_InFlightQueries = 0;
}

void CNetworkCrewDataMgr::DebugStartFetching()
{
	m_bDbgFetch = true;
}

void CNetworkCrewDataMgr::DebugUpdate()
{
	if( !m_bDbgFetch )
		return;

	const CNetworkCrewData* const pNewData = FindOrRequestData(m_DbgClanId);
	if( pNewData )
	{
		m_bDbgFetch = false;
		if( m_pDbgData != pNewData )
			DebugAddBankWidgets(pNewData);
	}
}

void CNetworkCrewDataMgr::DebugAddBankWidgets( const CNetworkCrewData* const pNewData )
{
	m_pDbgData = pNewData;
	bkBank* pBank = static_cast<bkBank*>(m_pDbgGroup->GetParent());
	m_pDbgGroup->Destroy();
	m_pDbgGroup = pBank->AddGroup("Fetched Data", true);
	if( pNewData )
	{
		// gotta do this to make the compiler happy
		CNetworkCrewData* pNonConst = const_cast<CNetworkCrewData*>(m_pDbgData);

		//m_pDbgGroup->AddText("ID",			static_cast<s32*>(&pNonConst->m_ClanInfo.m_Id),				true);
		m_pDbgGroup->AddTitle("DESCRIPTION");
		m_pDbgGroup->AddText("Name",		 pNonConst->m_ClanInfo[0].m_ClanName,	RL_CLAN_NAME_MAX_CHARS,	true);
		m_pDbgGroup->AddText("Tag",			 pNonConst->m_ClanInfo[0].m_ClanTag,	RL_CLAN_TAG_MAX_CHARS,	true);
		m_pDbgGroup->AddText("Motto",		 pNonConst->m_ClanInfo[0].m_ClanMotto,	RL_CLAN_MOTTO_MAX_CHARS, true);
		m_pDbgGroup->AddText("Members",		&pNonConst->m_ClanInfo[0].m_MemberCount,		true);
		m_pDbgGroup->AddText("Created",		&pNonConst->m_ClanInfo[0].m_CreatedTimePosix,	true);
		m_pDbgGroup->AddText("Is System",	&pNonConst->m_ClanInfo[0].m_IsSystemClan,		true);
		m_pDbgGroup->AddText("Is Open",		&pNonConst->m_ClanInfo[0].m_IsOpenClan,			true);
		//m_pDbgGroup->AddText("World Rank",	&pNonConst->m_LBSubset.WorldRank,				true);
		//m_pDbgGroup->AddText("World XP",	&pNonConst->m_LBSubset.WorldXP,					true);

		pNonConst->m_Metadata.DebugAddWidgets(m_pDbgGroup);
	
	}
}



#endif
