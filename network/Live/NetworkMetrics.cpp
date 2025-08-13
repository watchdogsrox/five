//
// NetworkMetrics.cpp
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#include "peds/PlayerInfo.h"

// Rage headers
#include "file/stream.h"
#include "net/nethardware.h"
#include "profile/timebars.h"
#include "data/rson.h"
#include "fragment/manager.h"
#include "atl/delegate.h"
#include "rline/scmembership/rlscmembership.h"

// Framework headers
#include "fwnet/netchannel.h"

//Network headers
#include "Network/Live/NetworkTelemetry.h"
#include "Network/Network.h"
#include "Network/NetworkInterface.h"
#include "Network/Sessions/NetworkGameConfig.h"
#include "Network/Sessions/NetworkSession.h"
#include "network/SocialClubServices/SocialClubFeedTilesMgr.h"
#include "network/SocialClubServices/SocialClubNewsStoryMgr.h"
#include "Network/Voice/NetworkVoice.h"

//Game headers
#include "frontend/landing_page/LandingPage.h"
#include "frontend/landing_page/LandingPageArbiter.h"
#include "frontend/page_deck/items/uiCloudSupport.h"
#include "scene/world/gameWorld.h"
#include "Stats/StatsInterface.h"
#include "Stats/MoneyInterface.h"
#include "system/FileMgr.h"
#include "network/xlast/Fuzzy.schema.h"
#include "NetworkMetrics_parser.h"

//IsNanInMemory
#include "phbound/surfacegrid.h"
#include "system/memops.h"
#include "string/stringutil.h"

NETWORK_OPTIMISATIONS()


/// SpendMetricCommon ---------------------------------------------------

int SpendMetricCommon::sm_properties = 0;
int SpendMetricCommon::sm_properties2 = 0;
int SpendMetricCommon::sm_heists = 0;
int SpendMetricCommon::sm_discount = 0;
#if NET_ENABLE_WINDFALL_METRICS
bool SpendMetricCommon::sm_windfall = false;
#endif

/// MetricPlayerCoords ---------------------------------------------------

MetricPlayerCoords::MetricPlayerCoords() : m_X(0.0f), m_Y(0.0f), m_Z(0.0f)
{
	const Vector3 coords = CGameWorld::FindLocalPlayer() ? CGameWorld::FindLocalPlayerCoors() : Vector3(0.0f, 0.0f, 0.0f);
	m_X = (IsNanInMemory(&coords.x)) ? 0.0f : coords.x;
	m_Y = (IsNanInMemory(&coords.y)) ? 0.0f : coords.y;
	m_Z = (IsNanInMemory(&coords.z)) ? 0.0f : coords.z;
}

bool MetricPlayerCoords::Write(RsonWriter* rw) const
{
	bool result = this->MetricPlayStat::Write(rw);

	result = result && rw->BeginArray("c", NULL);
	{
		result = result && rw->WriteFloat(NULL, m_X, "%.2f");
		result = result && rw->WriteFloat(NULL, m_Y, "%.2f");
		result = result && rw->WriteFloat(NULL, m_Z, "%.2f");
	}
	result = result && rw->End();

	return result;
}

void MetricPlayerCoords::MarkPlayerCoords()
{
	const Vector3 coords = CGameWorld::FindLocalPlayerCoors();
	m_X = coords.x;
	m_Y = coords.y;
	m_Z = coords.z;
}

/// MetricMultiplayerSubscription ---------------------------------------------------

bool MetricMultiplayerSubscription::Write(RsonWriter* rw) const
{
    bool result = this->MetricPlayStat::Write(rw);
    result = result && rw->WriteBool("Sub", m_HasSubscription);
    return result;
}

/// MetricTransitionTrack ---------------------------------------------------

void MetricTransitionTrack::Start(const int nType, const int nParam1, const int nParam2, const int nParam3)
{
	m_Type = nType;
	m_Param1 = nParam1;
	m_Param2 = nParam2;
	m_Param3 = nParam3;

	// create unique Id
	m_Identifier = static_cast<u32>(rlGetPosixTime());

	ClearStages();
}

void MetricTransitionTrack::ClearStages()
{
	m_nStages = 0;
	m_LastStageWipe = sysTimer::GetSystemMsTime();

	// dummy writer
	m_DummyWriter.Init(m_DummyBuffer, RSON_FORMAT_JSON);

	// we need to break up this metric, so track with a dummy metric so we know when to break this up
	MetricPlayStat::Write(&m_DummyWriter);
	m_DummyWriter.WriteUns("i", m_Identifier);
	m_DummyWriter.WriteInt("t", m_Type);
	m_DummyWriter.WriteInt("x", m_Param1);
	m_DummyWriter.WriteInt("y", m_Param2);
	m_DummyWriter.WriteInt("z", m_Param3);
	m_DummyWriter.BeginArray("s", NULL);
}

bool MetricTransitionTrack::AddStage(u32 nSlot, u32 nHash, int nParam1, int nParam2, int nParam3, u32 nPreviousTime)
{
	if(m_nStages >= MAX_TRANSITION_STAGES)
		return false;

	// apply previous time and write buffer
	if(m_nStages > 0)
	{
		m_data[m_nStages-1].m_TimeInMs = nPreviousTime;
		m_data[m_nStages-1].Write(&m_DummyWriter);
	}

	sTransitionStage stage(nSlot, nHash, nParam1, nParam2, nParam3);

	// dummy write so we can get the size
	char szStageBuffer[MAX_STAGE_WRITE_SIZE] = {0};
	RsonWriter tWriteCheck(szStageBuffer, RSON_FORMAT_JSON);
	stage.Write(&tWriteCheck);

	static const unsigned ADDITIONAL_CHARACTERS = 2;
	static const unsigned ADDITIONAL_BUFFER = 48;

	// check if this fits
	if(m_DummyWriter.Available() < (tWriteCheck.Length() + ADDITIONAL_CHARACTERS + ADDITIONAL_BUFFER))
	{
		// terminate the dummy writer
		m_DummyWriter.End();
		return false;
	}

	m_data[m_nStages] = stage;
	m_nStages++;

	return true; 
}

void MetricTransitionTrack::Finish()
{
	// fix any unset times
	for(u32 i = 0; i < m_nStages; i++)
	{
		if(m_data[i].m_TimeInMs == 0)
			m_data[i].m_TimeInMs = sysTimer::GetSystemMsTime() - ((i > 0) ? m_data[i-1].m_TimeInMs : m_LastStageWipe);
	}

	// we need to write our last stage
	if(m_nStages > 0)
		m_data[m_nStages-1].Write(&m_DummyWriter);

	m_DummyWriter.End();
}

MetricTransitionTrack::sTransitionStage& MetricTransitionTrack::sTransitionStage::operator=(const MetricTransitionTrack::sTransitionStage& other)
{
	m_Slot		= other.m_Slot;
	m_Hash		= other.m_Hash;
	m_TimeInMs  = other.m_TimeInMs;
	m_Param1	= other.m_Param1;
	m_Param2	= other.m_Param2;
	m_Param3	= other.m_Param3;

	return *this;
}

bool MetricTransitionTrack::sTransitionStage::Write(RsonWriter* rw) const
{
	bool result = true;

	rtry
	{
		rverify(rw->Begin(NULL, NULL), catchall, gnetError("sTransitionStage::Write :: Failed to begin"));
		{
			rverify(rw->WriteUns("s", m_Slot), catchall, gnetError("sTransitionStage::Write :: Failed to write m_Slot: %u", m_Slot));
			rverify(rw->WriteUns("h", m_Hash), catchall, gnetError("sTransitionStage::Write :: Failed to write m_Hash: %u", m_Hash));
			rverify(rw->WriteUns("t", m_TimeInMs), catchall, gnetError("sTransitionStage::Write :: Failed to write m_TimeInMs: %u", m_TimeInMs));
			if(m_Param1 != 0)
				rverify(rw->WriteInt("x", m_Param1), catchall, gnetError("sTransitionStage::Write :: Failed to write m_Param1: %d", m_Param1));
			if(m_Param2 != 0)
				rverify(rw->WriteInt("y", m_Param2), catchall, gnetError("sTransitionStage::Write :: Failed to write m_Param2: %d", m_Param2));
			if(m_Param3 != 0)
				rverify(rw->WriteInt("z", m_Param3), catchall, gnetError("sTransitionStage::Write :: Failed to write m_Param3: %d", m_Param3));
		}
		rverify(rw->End(), catchall, gnetError("sTransitionStage::Write :: Failed to end"));
	}
	rcatchall
	{
		result = false;
	}

	return result;
}

bool MetricTransitionTrack::Write(RsonWriter* rw) const
{
	bool result = true;

	rtry
	{
		rverify(this->MetricPlayStat::Write(rw), catchall, gnetError("MetricTransitionTrack::Write :: Failed to write MetricPlayStat"));
		{
			rverify(rw->WriteUns("i", m_Identifier), catchall, gnetError("MetricTransitionTrack::Write :: Failed to write m_Identifier %u", m_Identifier));
			rverify(rw->WriteInt("t", m_Type), catchall, gnetError("MetricTransitionTrack::Write :: Failed to write m_Type %d", m_Type));
			rverify(rw->WriteInt("x", m_Param1), catchall, gnetError("MetricTransitionTrack::Write :: Failed to write m_Param1: %d", m_Param1));
			rverify(rw->WriteInt("y", m_Param2), catchall, gnetError("MetricTransitionTrack::Write :: Failed to write m_Param2: %d", m_Param2));
			rverify(rw->WriteInt("z", m_Param3), catchall, gnetError("MetricTransitionTrack::Write :: Failed to write m_Param3: %d", m_Param3));
			
			rverify(rw->BeginArray("s", NULL), catchall, gnetError("MetricTransitionTrack :: Failed to begin stage array"));
			for(u32 i = 0; i < m_nStages; i++)
				rcheck(m_data[i].Write(rw), catchall, gnetError("MetricTransitionTrack :: Failed to write candidate %u", i));
			rverify(rw->End(), catchall, gnetError("MetricTransitionTrack::Write :: Failed to end"));
		}
	}
	rcatchall
	{
		result = false;
	}

	return result;
}

void MetricTransitionTrack::LogContents()
{
#if RSG_OUTPUT
	diagLoggedPrintLn(m_DummyWriter.ToString(), m_DummyWriter.Length());
#endif
}


/// MetricMatchmakingQueryResults ---------------------------------------------------

bool MetricMatchmakingQueryResults::AddCandidate(const sCandidateData& data)
{
	if(m_nCandidates >= NETWORK_MAX_MATCHMAKING_CANDIDATES)
		return false;

	m_Candidates[m_nCandidates] = data;
	m_nCandidates++;

	return true; 
}

MetricMatchmakingQueryResults::sCandidateData& MetricMatchmakingQueryResults::sCandidateData::operator=(const MetricMatchmakingQueryResults::sCandidateData& other)
{
	m_sessionid     = other.m_sessionid;
	m_usedSlots     = other.m_usedSlots;
	m_maxSlots      = other.m_maxSlots;
	m_TimeAttempted	= other.m_TimeAttempted;
	m_ResultCode	= other.m_ResultCode;
	m_fromQueries	= other.m_fromQueries;

	return *this;
}

bool MetricMatchmakingQueryResults::sCandidateData::Write(RsonWriter* rw) const
{
	bool result = true;

	rtry
	{
		rverify(rw->Begin(NULL, NULL), catchall, gnetError("sCandidateData::Write :: Failed to begin"));
		{
			rverify(rw->WriteInt64("i", static_cast<s64>(m_sessionid)), catchall, gnetError("sCandidateData::Write :: Failed to write sid: 0x%016" I64FMT "x", m_sessionid));
			rverify(rw->WriteUns("q", m_fromQueries), catchall, gnetError("sCandidateData::Write :: Failed to write from queries: %u", m_fromQueries));
			rverify(rw->WriteUns("u", m_usedSlots), catchall, gnetError("sCandidateData::Write :: Failed to write usedslots: %u", m_usedSlots));
			rverify(rw->WriteUns("m", m_maxSlots), catchall, gnetError("sCandidateData::Write :: Failed to write maxslots: %u", m_maxSlots));
			rverify(rw->WriteUns("t", m_TimeAttempted), catchall, gnetError("sCandidateData::Write :: Failed to write time: %u", m_TimeAttempted));
			rverify(rw->WriteUns("r", m_ResultCode), catchall, gnetError("sCandidateData::Write :: Failed to write result: %u", m_ResultCode));
		}
		rverify(rw->End(), catchall, gnetError("sCandidateData::Write :: Failed to end"));
	}
	rcatchall
	{
		result = false;
	}

	return result;
}

bool MetricMatchmakingQueryResults::Write(RsonWriter* rw) const
{
	bool result = true;

	rtry
	{
		rverify(this->MetricPlayStat::Write(rw), catchall, gnetError("MetricMatchmakingQueryResults::Write :: Failed to write MetricPlayStat"));
		{
			rverify(rw->WriteInt64("u", static_cast<s64>(m_UniqueID)), catchall, gnetError("MetricMatchmakingQueryResults::Write :: Failed to write uid (u): 0x%016" I64FMT "x", m_UniqueID));
			rverify(rw->WriteUns("m", m_nGameMode), catchall, gnetError("MetricMatchmakingQueryResults::Write :: Failed to write game mode (m): %u", m_nGameMode));
			rverify(rw->WriteUns("c", m_CandidateIndex), catchall, gnetError("MetricMatchmakingQueryResults::Write :: Failed to write candidate index (c): %u", m_CandidateIndex));
			rverify(rw->WriteUns("t", m_TimeTaken), catchall, gnetError("MetricMatchmakingQueryResults::Write :: Failed to write time taken (t): %u", m_TimeTaken));
			rverify(rw->WriteBool("h", m_bHosted), catchall, gnetError("MetricMatchmakingQueryResults::Write :: Failed to write hosted (h): %s", m_bHosted ? "True" : "False"));
			rverify(rw->WriteUns("n", m_nCandidates), catchall, gnetError("MetricMatchmakingQueryResults::Write :: Failed to write number of candidates (n): %u", m_nCandidates));

			rverify(rw->BeginArray("q", NULL), catchall, gnetError("MetricMatchmakingQueryResults :: Failed to begin candidate array"));
			for(u32 i = 0; i < m_nCandidates; i++)
			{
				// we don't want to fail to metric if one candidate won't write so write to a temporary buffer to 
				// check that our metric buffer has enough space to contain the metric
				char szCandidateBuffer[128] = {0};
				RsonWriter tWriteCheck(szCandidateBuffer, RSON_FORMAT_JSON);
				m_Candidates[i].Write(&tWriteCheck);

				static const unsigned ADDITIONAL_CHARACTERS = 2;
				static const unsigned ADDITIONAL_BUFFER = 0;

				// check if this fits
				if(rw->Available() >= (tWriteCheck.Length() + ADDITIONAL_CHARACTERS + ADDITIONAL_BUFFER))
				{
					rcheck(m_Candidates[i].Write(rw), catchall, gnetError("MetricMatchmakingQueryResults :: Failed to write candidate %u", i));
				}
				else
				{
					gnetDebug1("MetricMatchmakingQueryResults :: Skipping candidate %u, no space", i);
				}
			}
			rverify(rw->End(), catchall, gnetError("sCandidateData::Write :: Failed to end"));
		}
	}
	rcatchall
	{
		result = false;
	}

	return result;
}


/// MetricMatchmakingQueryStart ---------------------------------------------------

bool MetricMatchmakingQueryStart::AddFilterValue(const u32 id, const u32* value)
{
	bool result = false;

	if (value)
	{
		for (int i=0; i<MAX_NUM_FILTERIDS && !result; i++)
		{
			if (m_data[i].m_id == id)
			{
				gnetAssert(*value == m_data[i].m_value);
				result = true;
				break;
			}
			else if (m_data[i].m_id == sData::INVALID_ID)
			{
				m_data[i].m_id = id;
				m_data[i].m_value = *value;
				result = true;
				break;
			}
		}

		gnetAssert(result);
	}

	return result;
}

bool MetricMatchmakingQueryStart::sData::Write(RsonWriter* rw) const
{
	bool result = false;

	if (rw)
	{
		result = true;

		if (INVALID_ID != m_id)
		{
			result = result && rw->Begin(NULL,NULL);
			result = result && rw->WriteUns("i", m_id);
			result = result && rw->WriteUns("v", m_value);
			result = result && rw->End();
		}
	}

	return result;
}

bool MetricMatchmakingQueryStart::Write(RsonWriter* rw) const
{
	bool result = this->MetricPlayStat::Write(rw);

	result = result && rw->WriteUns("gm", m_gamemode);
	result = result && rw->WriteInt64("uid", static_cast<s64>(m_uid));
	result = result && rw->WriteUns("qm", m_queriesMade);

	result = result && rw->BeginArray("q", NULL);
	for(u32 i=0; i<MAX_NUM_FILTERIDS && result; i++)
	{
		result = result && m_data[i].Write(rw);
	}
	result = result && rw->End();

	return result;
}

/// MetricJoinSessionFailed ---------------------------------------------------

bool MetricJoinSessionFailed::Write(RsonWriter* rw) const
{
	bool result = this->MetricPlayStat::Write(rw);
	result = result && rw->WriteInt64("sid", m_sessionid);
	result = result && rw->WriteInt64("uid", static_cast<s64>(m_uid));
	result = result && rw->WriteInt("bail", m_bailReason);
	return result;
}


/// MetricMpSession ---------------------------------------------------

MetricMpSession::MetricMpSession(const bool isTransitioning)
: MetricMpSessionId(CNetworkTelemetry::GetCurMpSessionId(), isTransitioning)
, m_GameMode(CNetworkTelemetry::GetCurMpGameMode())
, m_PubSlots(CNetworkTelemetry::GetCurMpNumPubSlots())
, m_PrivSlots(CNetworkTelemetry::GetCurMpNumPrivSlots())
{

}

/// MetricJOB_STARTED ---------------------------------------------------

MetricJOB_STARTED::MetricJOB_STARTED(const char* activity, const sMatchStartInfo& info)
	: m_numFriendsTotal(info.m_numFriendsTotal)
	, m_numFriendsInSameTitle(info.m_numFriendsInSameTitle)
	, m_numFriendsInMySession(info.m_numFriendsInMySession)
	, m_oncalljointime(info.m_oncalljointime)
	, m_oncalljoinstate(info.m_oncalljoinstate)
	, m_missionDifficulty(info.m_missionDifficulty)
	, m_MissionLaunch(info.m_MissionLaunch)
	, m_usedVoiceChat(CNetworkTelemetry::HasUsedVoiceChatSinceLastTime())
{
	safecpy(m_activity, activity, COUNTOF(m_activity));
}

/// MetricJOB_ENDED ---------------------------------------------------

MetricJOB_ENDED::MetricJOB_ENDED(const sJobInfo& jobInfo, const char* playlistid) 
: MetricMpSession(false)
, m_matchmakingKey(NetworkBaseConfig::GetMatchmakingUser(false))
, m_exeSize((s64)NetworkInterface::GetExeSize( ))
, m_usedVoiceChat(CNetworkTelemetry::HasUsedVoiceChatSinceLastTime())
{
	m_jobInfo = jobInfo;

	const CNetworkSession::sUniqueCrewData& crewData = CNetwork::GetNetworkSession().GetMainSessionCrewData();
	m_CrewLimit = crewData.m_nLimit;

	sysMemSet(m_playlistid, 0, COUNTOF(m_playlistid));
	if (playlistid)
		safecpy(m_playlistid, playlistid, COUNTOF(m_playlistid));

	m_closedJob = CNetwork::GetNetworkSession().IsClosedSession(SessionType::ST_Physical);
	m_privateJob = CNetwork::GetNetworkSession().IsPrivateSession(SessionType::ST_Physical);
	CNetworkTelemetry::GetIsClosedAndPrivateForLastFreemode(m_fromClosedFreemode, m_fromPrivateFreemode);
}

bool MetricJOB_ENDED::Write(RsonWriter* rw) const
{
	bool result = MetricMpSession::Write(rw);

	result = result && rw->WriteUns("mtkey", m_matchmakingKey);

	result = result && rw->WriteInt64("exe", m_exeSize);

	result = result && rw->WriteString("mc", m_jobInfo.m_MatchCreator);

	result = result && rw->WriteString("mid", m_jobInfo.m_UniqueMatchId);

	result = result && rw->WriteBool("vc", m_usedVoiceChat);

	result = result && rw->WriteBool("cj", m_closedJob);
	result = result && rw->WriteBool("cfm", m_fromClosedFreemode);
	result = result && rw->WriteBool("pj", m_privateJob);
	result = result && rw->WriteBool("pfm", m_fromPrivateFreemode);

	if (m_jobInfo.m_intsused>0)
	{
		result = result && rw->BeginArray("i", NULL);
		{
			for(u32 i=0; i<m_jobInfo.m_intsused; i++)
				result = result && rw->WriteInt(NULL, m_jobInfo.m_dataINTS[i]);
		}
		result = result && rw->End();
	}

	if (m_jobInfo.m_boolused>0)
	{
		result = result && rw->BeginArray("b", NULL);
		{
			for(u32 i=0; i<m_jobInfo.m_boolused; i++)
				result = result && rw->WriteBool(NULL, m_jobInfo.m_dataBOOLEANS[i]);
		}
		result = result && rw->End();
	}

	if (m_jobInfo.m_u32used>0)
	{
		result = result && rw->BeginArray("u", NULL);
		{
			for(u32 i=0; i<m_jobInfo.m_u32used; i++)
				result = result && rw->WriteUns(NULL, m_jobInfo.m_dataU32[i]);
		}
		result = result && rw->End();

	}

	if (m_jobInfo.m_u64used>0)
	{
		result = result && rw->BeginArray("u64", NULL);
		{
			for(u32 i=0; i<m_jobInfo.m_u64used; i++)
				result = result && rw->WriteInt64(NULL, static_cast<s64>(m_jobInfo.m_dataU64[i]));
		}
		result = result && rw->End();
	}

	//Value != 0 means Crew Only sessions.
	result = result && rw->WriteUns("limit", m_CrewLimit);

	result = result && rw->WriteString("plid", m_playlistid);

	return result;
}


/// MetricSessionStart ---------------------------------------------------

MetricSessionStart::MetricSessionStart(bool isTransition) 
	: m_Nat(rage::netHardware::GetNatType())
	, m_isTransition(isTransition)
{
	rage::netIpAddress myIp;
	rage::netHardware::GetPublicIpAddress(&myIp);
	if(myIp.IsV4())
		myIp.ToV4().Format(m_LocalIp);
	else
		myIp.Format(m_LocalIp);

	const rage::netAddress& myRelay = rage::netRelay::GetMyRelayAddress();

	m_UsedRelay = myRelay.IsRelayServerAddr();
	myRelay.GetProxyAddress().FormatAttemptIpV4(m_RelayIp, rage::netSocketAddress::MAX_STRING_BUF_SIZE, false);
}

bool MetricSessionStart::Write(RsonWriter* rw) const
{
	bool result;
	result = rw->WriteBool("tr", m_isTransition)
		&& rw->WriteString("ip", m_LocalIp)
		&& rw->WriteString("rip", m_RelayIp)
		&& rw->WriteBool("usedr", m_UsedRelay)
		&& rw->WriteInt("nat", m_Nat);
	return result;
}

/// MetricInviteResult ---------------------------------------------

MetricInviteResult::MetricInviteResult(
	const u64 sessionToken, 
	const char* uniqueMatchId, 
	const int gameMode, 
	const unsigned inviteFlags,
	const unsigned inviteId, 
	const unsigned inviteAction,
	const int inviteFrom,
	const int inviteType, 
	const rlGamerHandle& inviterHandle, 
	const bool isFriend)
	: m_SessionToken(sessionToken)
	, m_GameMode(gameMode)
	, m_InviteFlags(inviteFlags)
	, m_InviteId(inviteId)
	, m_InviteAction(inviteAction)
	, m_InviteFrom(inviteFrom)
	, m_InviteType(inviteType)
	, m_Inviter(inviterHandle)
	, m_IsFromFriend(isFriend)
{
	safecpy(m_UniqueMatchId, uniqueMatchId);
}

bool MetricInviteResult::Write(RsonWriter* rw) const
{
	bool result = MetricPlayStat::Write(rw);
	result = result && rw->WriteInt64("st", static_cast<s64>(m_SessionToken));
	result = result && rw->WriteString("mid", m_UniqueMatchId);
	result = result && rw->WriteInt("m", m_GameMode);
	result = result && rw->WriteUns("flags", m_InviteFlags);
	result = result && rw->WriteUns("iid", m_InviteId);
	result = result && rw->WriteUns("ia", m_InviteAction);
	result = result && rw->WriteInt("if", m_InviteFrom);
	result = result && rw->WriteInt("it", m_InviteType);
	if(m_Inviter.IsValid())
	{
		char szInviterUserId[RL_MAX_USERID_BUF_LENGTH];
		m_Inviter.ToUserId(szInviterUserId);
		result = result && rw->WriteString("inviter", szInviterUserId);
	}
	result = result && rw->WriteBool("isfriend", m_IsFromFriend);
	return result;
}

/// MetricENTERED_SOLO_SESSION ---------------------------------------------

MetricENTERED_SOLO_SESSION::MetricENTERED_SOLO_SESSION(const u64 sessionId, const u64 sessionToken, const int gameMode, const eSoloSessionReason soloSessionReason, const eSessionVisibility sessionVisibility, const eSoloSessionSource soloSessionSource, const int userParam1, const int userParam2, const int userParam3)
	: m_SessionId(sessionId)
	, m_SessionToken(sessionToken)
	, m_GameMode(gameMode)
	, m_SoloSessionReason(soloSessionReason)
	, m_SessionVisibility(sessionVisibility)
	, m_SoloSessionSource(soloSessionSource)
	, m_UserParam1(userParam1)
	, m_UserParam2(userParam2)
	, m_UserParam3(userParam3)
{

}

bool MetricENTERED_SOLO_SESSION::Write(RsonWriter* rw) const
{
	bool result = MetricPlayStat::Write(rw);
	result = result && rw->WriteInt64("sid", static_cast<s64>(m_SessionId));
	result = result && rw->WriteInt64("tok", static_cast<s64>(m_SessionToken));
	result = result && rw->WriteInt("gm", m_GameMode);
	result = result && rw->WriteUns("rsn", static_cast<unsigned>(m_SoloSessionReason));
	result = result && rw->WriteUns("vis", static_cast<unsigned>(m_SessionVisibility));
	result = result && rw->WriteUns("src", static_cast<unsigned>(m_SoloSessionSource));
	result = result && rw->WriteInt("prm", m_UserParam1);
	result = result && rw->WriteInt("up2", m_UserParam2);
	result = result && rw->WriteInt("up3", m_UserParam3);
	return result;
}

/// MetricSTALL_DETECTED ---------------------------------------------

MetricSTALL_DETECTED::MetricSTALL_DETECTED(const unsigned stallTimeMs, const unsigned networkTimeMs, const bool bIsTransitioning, const bool bIsLaunching, const bool bIsHost, const unsigned nTimeoutsPending, const unsigned sessionSize, const u64 sessionToken, const u64 sessionId)
	: m_StallTimeMs(stallTimeMs)
	, m_NetworkTimeMs(networkTimeMs)
	, m_bInTransition(bIsTransitioning)
	, m_bIsLaunching(bIsLaunching)
	, m_bIsHost(bIsHost)
	, m_NumTimeoutsPending(nTimeoutsPending)
	, m_bInMultiplayer(true)
	, m_SessionSize(sessionSize)
	, m_SessionToken(sessionToken)
	, m_SessionId(sessionId)
{

}

MetricSTALL_DETECTED::MetricSTALL_DETECTED(const unsigned stallTimeMs, const unsigned networkTimeMs)
	: m_StallTimeMs(stallTimeMs)
	, m_NetworkTimeMs(networkTimeMs)
	, m_bInTransition(false)
	, m_bIsLaunching(false)
	, m_bInMultiplayer(false)
	, m_bIsHost(false)
	, m_NumTimeoutsPending(0)
	, m_SessionSize(0)
	, m_SessionToken(0)
	, m_SessionId(0)
{

}

bool MetricSTALL_DETECTED::Write(RsonWriter* rw) const
{
	bool result = MetricPlayStat::Write(rw);
	result = result && rw->WriteUns("stime", m_StallTimeMs);
	result = result && rw->WriteUns("ntime", m_NetworkTimeMs);
	result = result && rw->WriteBool("inmp", m_bInMultiplayer);
	result = result && rw->WriteBool("trn", m_bInTransition);
	result = result && rw->WriteBool("ln", m_bIsLaunching);
	result = result && rw->WriteBool("h", m_bIsHost);
	result = result && rw->WriteUns("top", m_NumTimeoutsPending);
	result = result && rw->WriteUns("ss", m_SessionSize);
	result = result && rw->WriteInt64("st", static_cast<s64>(m_SessionToken));
	result = result && rw->WriteInt64("sid", static_cast<s64>(m_SessionId));
	return result;
}

/// MetricNETWORK_BAIL ---------------------------------------------

MetricNETWORK_BAIL::MetricNETWORK_BAIL(const sBailParameters bailParams,
									   const unsigned nSessionState,
									   const u64 sessionId,
									   const bool bIsHost,
									   const bool bJoiningViaMatchmaking,
									   const bool bJoiningViaInvite,
									   const unsigned nTime)
	: m_bailParams(bailParams)
	, m_nSessionState(nSessionState)
	, m_sessionId(sessionId)
	, m_bIsHost(bIsHost)	
	, m_bJoiningViaMatchmaking(bJoiningViaMatchmaking)
	, m_bJoiningViaInvite(bJoiningViaInvite)
	, m_nTime(nTime)
{

}

bool MetricNETWORK_BAIL::Write(RsonWriter* rw) const
{
	bool result = MetricPlayStat::Write(rw);
	result = result && rw->WriteUns("b", m_bailParams.m_BailReason);
	result = result && rw->WriteInt("c1", m_bailParams.m_ContextParam1);
	result = result && rw->WriteInt("c2", m_bailParams.m_ContextParam2);
	result = result && rw->WriteInt("c3", m_bailParams.m_ContextParam3);
	result = result && rw->WriteUns("s", static_cast<unsigned>(m_nSessionState));
	result = result && rw->WriteInt64("sid", static_cast<s64>(m_sessionId));
	result = result && rw->WriteBool("h", m_bIsHost);
	result = result && rw->WriteBool("mm", m_bJoiningViaMatchmaking);
	result = result && rw->WriteBool("i", m_bJoiningViaInvite);
	result = result && rw->WriteUns("t", m_nTime);
	result = result && rw->WriteUns("bc", m_bailParams.m_ErrorCode);
	return result;
}

/// MetricNETWORK_KICKED ---------------------------------------------

MetricNETWORK_KICKED::MetricNETWORK_KICKED(const u64 nSessionToken, 
										   const u64 nSessionId, 
										   const u32 nGameMode, 
										   const u32 nSource, 
										   const u32 nPlayers, 
										   const u32 nSessionState, 
										   const u32 nTimeInSession, 
										   const u32 nTimeSinceLastMigration, 
										   const unsigned nNumComplaints, 
										   const rlGamerHandle& hGamer)
	: m_SessionToken(nSessionToken)
	, m_SessionId(nSessionId)
	, m_nGameMode(nGameMode)
	, m_nSource(nSource)
	, m_nPlayers(nPlayers)
	, m_nSessionState(nSessionState)
	, m_nTimeInSession(nTimeInSession)
	, m_nTimeSinceLastMigration(nTimeSinceLastMigration)
	, m_NumComplaints(nNumComplaints)
{
	if(hGamer.IsValid())
		hGamer.ToUserId(m_szComplainerUserID);
	else
		m_szComplainerUserID[0] = '\0';
}

bool MetricNETWORK_KICKED::Write(RsonWriter* rw) const
{
	bool result = MetricPlayStat::Write(rw);
	result = result && rw->WriteInt64("st", static_cast<s64>(m_SessionToken));
	result = result && rw->WriteInt64("sid", static_cast<s64>(m_SessionId));
	result = result && rw->WriteUns("m", m_nGameMode);
	result = result && rw->WriteUns("s", m_nSource);
	result = result && rw->WriteUns("p", m_nPlayers);
	result = result && rw->WriteUns("ss", m_nSessionState);
	result = result && rw->WriteUns("ts", m_nTimeInSession);
	result = result && rw->WriteUns("tm", m_nTimeSinceLastMigration);
	result = result && rw->WriteUns("mp", 0);
	result = result && rw->WriteUns("c", m_NumComplaints);
	result = result && rw->WriteString("cid", m_szComplainerUserID);
	return result;
}

MetricSessionMigrated::MetricSessionMigrated()
	: m_GameMode(static_cast<u32>(MultiplayerGameMode::GameMode_Invalid))
	, m_MigrationId(0)
	, m_Time(0)
	, m_NumCandidates(0)
{

}

void MetricSessionMigrated::Start(const u64 nMigrationId, const u32 nPosixTime, const u32 nGameMode)
{
	m_MigrationId = nMigrationId; 
	m_PosixTime = nPosixTime;
	m_GameMode = nGameMode;
	m_Time = 0;
	m_NumCandidates = 0;
}

void MetricSessionMigrated::AddCandidate(const char* szGamerName, const CandidateCode nCode)
{
	if(!gnetVerifyf(m_NumCandidates < RL_MAX_GAMERS_PER_SESSION, "MetricSessionMigrated::AddCandidate :: Too many candidates!"))
		return; 

	// assume disconnected - we'll set the result code for candidates who remain 
	safecpy(m_Candidates[m_NumCandidates].m_Name, szGamerName);
	m_Candidates[m_NumCandidates].m_Code = nCode;
	m_Candidates[m_NumCandidates].m_Result = CANDIDATE_RESULT_DISCONNECTED;
	m_NumCandidates++;
}

void MetricSessionMigrated::SetCandidateResult(const char* szGamerName, const CandidateResult nResult)
{
	// find this gamer and apply the result
	for(unsigned i = 0; i < m_NumCandidates; i++)
	{
		if(strncmp(m_Candidates[i].m_Name, szGamerName, RL_MAX_NAME_BUF_SIZE) == 0)
		{
			m_Candidates[i].m_Result = nResult;
			return;
		}
	}

	// still here, this is a new player
	if(!gnetVerifyf(m_NumCandidates < RL_MAX_GAMERS_PER_SESSION, "MetricSessionMigrated::SetCandidateResult :: Too many candidates!"))
		return; 

	// add the new candidate 
	safecpy(m_Candidates[m_NumCandidates].m_Name, szGamerName);
	m_Candidates[m_NumCandidates].m_Code = CANDIDATE_CODE_UNKNOWN;
	m_Candidates[m_NumCandidates].m_Result = (nResult == CANDIDATE_RESULT_CLIENT) ? CANDIDATE_RESULT_ADDED_CLIENT : CANDIDATE_RESULT_ADDED_HOST;
	m_NumCandidates++;
}

void MetricSessionMigrated::Finish(const u32 nTimeTaken)
{
	m_Time = nTimeTaken; 
}

bool MetricSessionMigrated::sCandidate::Write(RsonWriter* rw) const
{
	bool result = true;

	rtry
	{
		rverify(rw->Begin(NULL, NULL), catchall, gnetError("sCandidate::Write :: Failed to begin"));
		{
			rverify(rw->WriteString("n", m_Name), catchall, gnetError("sCandidate::Write :: Failed to write m_Name (n): %s", m_Name));
			rverify(rw->WriteUns("c", static_cast<unsigned>(m_Code)), catchall, gnetError("sCandidate::Write :: Failed to write m_Code (c): %u", static_cast<unsigned>(m_Code)));
			rverify(rw->WriteUns("r", static_cast<unsigned>(m_Result)), catchall, gnetError("sCandidate::Write :: Failed to write m_Result (r): %u", static_cast<unsigned>(m_Result)));
		}
		rverify(rw->End(), catchall, gnetError("sCandidateData::Write :: Failed to end"));
	}
	rcatchall
	{
		result = false;
	}

	return result;
}

bool MetricSessionMigrated::Write(RsonWriter* rw) const
{
	bool result = true;

	rtry
	{
		rverify(this->MetricPlayStat::Write(rw), catchall, gnetError("MetricSessionMigrated::Write :: Failed to write MetricPlayStat"));
		{
			rverify(rw->WriteInt64("i", static_cast<s64>(m_MigrationId)), catchall, gnetError("MetricSessionMigrated::Write :: Failed to write m_MigrationId (i): 0x%016" I64FMT "x", m_MigrationId));
			rverify(rw->WriteUns("p", m_PosixTime), catchall, gnetError("MetricSessionMigrated::Write :: Failed to write m_PosixTime (p): %u", m_PosixTime));
			rverify(rw->WriteUns("m", m_GameMode), catchall, gnetError("MetricSessionMigrated::Write :: Failed to write m_GameMode (m): %u", m_GameMode));
			rverify(rw->WriteUns("t", m_Time), catchall, gnetError("MetricSessionMigrated::Write :: Failed to write m_Time (t): %u", m_Time));

			rverify(rw->BeginArray("c", NULL), catchall, gnetError("MetricSessionMigrated :: Failed to begin candidate array"));
			for(u32 i = 0; i < m_NumCandidates; i++)
			{
				// we don't want to fail to metric if one candidate won't write so write to a temporary buffer to 
				// check that our metric buffer has enough space to contain the metric
				char szCandidateBuffer[128] = {0};
				RsonWriter tWriteCheck(szCandidateBuffer, RSON_FORMAT_JSON);
				m_Candidates[i].Write(&tWriteCheck);

				static const unsigned ADDITIONAL_CHARACTERS = 2;
				static const unsigned ADDITIONAL_BUFFER = 2;

				// check if this fits
				if(rw->Available() >= (tWriteCheck.Length() + ADDITIONAL_CHARACTERS + ADDITIONAL_BUFFER))
				{
					rcheck(m_Candidates[i].Write(rw), catchall, gnetError("MetricSessionMigrated :: Failed to write candidate %u", i));
				}
				else
				{
					gnetDebug1("MetricSessionMigrated :: Skipping candidate %u (%s), no space", i, m_Candidates[i].m_Name);
				}
			}
			rverify(rw->End(), catchall, gnetError("MetricSessionMigrated::Write :: Failed to end"));
		}
	}
	rcatchall
	{
		result = false;
	}

	return result;
}

/// MetricSESSION_HOSTED ---------------------------------------------------

MetricSESSION_HOSTED::MetricSESSION_HOSTED(const bool isTransitioning, const u32 matchmakingKey, const u64 nUniqueMatchmakingID, const u64 sessionToken, const u64 nTimeCreated) 
	: MetricMpSession(isTransitioning)
	, m_matchmakingKey(matchmakingKey)
	, m_exeSize((s64) NetworkInterface::GetExeSize( ))
	, m_OnlineJoinType(CNetworkTelemetry::GetOnlineJoinType())
	, m_nUniqueMatchmakingID(nUniqueMatchmakingID)
	, m_start(isTransitioning)
	, m_nSessionToken(sessionToken)
	, m_nTimeCreated(nTimeCreated)
{;}

bool MetricSESSION_HOSTED::Write(RsonWriter* rw) const
{
	bool result = MetricMpSession::Write(rw);
	result = result && m_start.Write(rw);
	result = result && rw->WriteUns("mtkey", m_matchmakingKey);
	result = result && rw->WriteInt64("exe", m_exeSize);
	result = result && rw->WriteUns("jt", m_OnlineJoinType);
	result = result && rw->WriteBool("dlc", CFileMgr::IsDownloadableVersion());

	// not sure if this is still used for anything, but removed all hardcoded ps3-only sku output
	result = result && WriteSku(rw);

	result = result && rw->WriteInt64("uid", static_cast<s64>(m_nUniqueMatchmakingID));
	result = result && rw->WriteInt64("sst", static_cast<s64>(m_nSessionToken));
	result = result && rw->WriteInt64("tc", static_cast<s64>(m_nTimeCreated));

	return result;
}

/// MetricSESSION_JOINED ---------------------------------------------------

MetricSESSION_JOINED::MetricSESSION_JOINED( const bool isTransitioning, const unsigned matchmakingKey, const u64 nUniqueMatchmakingID, const u64 sessionToken, const u64 nTimeCreated) 
	: MetricMpSession(isTransitioning)
	, m_numPlayers(NetworkInterface::GetNumActivePlayers())
	, m_matchmakingKey(matchmakingKey) 
	, m_exeSize((s64) NetworkInterface::GetExeSize( ))
	, m_sctv( CNetwork::GetNetworkSession().GetMatchmakingGroup() == MM_GROUP_SCTV )
	, m_OnlineJoinType(CNetworkTelemetry::GetOnlineJoinType())
	, m_nUniqueMatchmakingID(nUniqueMatchmakingID)
	, m_start(isTransitioning)
	, m_nSessionToken(sessionToken)
	, m_nTimeCreated(nTimeCreated)
{

}

bool MetricSESSION_JOINED::Write(RsonWriter* rw) const
{
	bool result = MetricMpSession::Write(rw);
	result = result && m_start.Write(rw);
	result = result && rw->WriteUns("p", m_numPlayers);
	result = result && rw->WriteUns("mtkey", m_matchmakingKey);
	result = result && rw->WriteInt64("exe", m_exeSize);
	result = result && rw->WriteBool("sctv", m_sctv);
	result = result && rw->WriteUns("jt", m_OnlineJoinType);

	result = result && rw->WriteBool("dlc", CFileMgr::IsDownloadableVersion());

	// not sure if this is still used for anything, but removed all hardcoded ps3-only sku output
	result = result && WriteSku(rw);

	result = result && rw->WriteInt64("uid", static_cast<s64>(m_nUniqueMatchmakingID));
	result = result && rw->WriteInt64("sst", static_cast<s64>(m_nSessionToken));
	result = result && rw->WriteInt64("tc", static_cast<s64>(m_nTimeCreated));

	return result;
}

/// MetricSESSION_LEFT ---------------------------------------------------

MetricSESSION_LEFT::MetricSESSION_LEFT(const u64 sessionId, const bool isTransitioning) : MetricMpSessionId(sessionId, isTransitioning)
, m_numPlayers(NetworkInterface::GetNumActivePlayers()) 
{
}

bool MetricSESSION_LEFT::Write(RsonWriter* rw) const
{
	return MetricMpSessionId::Write(rw) 
		&& rw->WriteUns("p", m_numPlayers);
}


/// MetricAMBIENT_MISSION_CRATEDROP ---------------------------------------------------

MetricBaseMultiplayerMissionDone::MetricBaseMultiplayerMissionDone(const u32 missionId, const u32 xpEarned, const u32 cashEarned, const u32 cashSpent)
	: m_MissionId(missionId)
	, m_XpEarned(xpEarned)
	, m_CashEarned(cashEarned)
	, m_CashSpent(cashSpent)
	, m_playtime(0)
{
	m_playtime = (u32)StatsInterface::GetUInt64Stat(STAT_TOTAL_PLAYING_TIME.GetHash());
}

bool MetricBaseMultiplayerMissionDone::Write(RsonWriter* rw) const
{
	bool result = rw->WriteUns("id", m_MissionId);

	result = result && rw->WriteUns("pt", m_playtime);

	if (m_XpEarned > 0)
		result = result && rw->WriteUns("xp", m_XpEarned);

	if (m_CashEarned > 0)
		result = result && rw->WriteUns("ce", m_CashEarned);

	if (m_CashSpent > 0)
		result = result && rw->WriteUns("cs", m_CashSpent);

	return result;
}

/// MetricAMBIENT_MISSION_CRATEDROP ---------------------------------------------------

MetricAMBIENT_MISSION_CRATEDROP::MetricAMBIENT_MISSION_CRATEDROP(const u32 missionId, const u32 xpEarned, const u32 cashEarned, const u32 weaponHash, const u32 otherItem, const u32 enemiesKilled, const u32 (&_SpecialItemHashs)[MAX_CRATE_ITEM_HASHS], bool _CollectedBodyArmour)
	: MetricBaseMultiplayerMissionDone(missionId, xpEarned, cashEarned, 0)
	, m_WeaponHash(weaponHash)
	, m_OtherItem(otherItem)
	, m_EnemiesKilled(enemiesKilled)
	, m_CollectedBodyArmour(_CollectedBodyArmour)
{
	gnetAssert(xpEarned>0 || cashEarned>0 || weaponHash>0 || otherItem>0);
	memcpy(m_SpecialItemHashs, _SpecialItemHashs, sizeof(u32)*MetricAMBIENT_MISSION_CRATEDROP::MAX_CRATE_ITEM_HASHS);
}

bool MetricAMBIENT_MISSION_CRATEDROP::Write(RsonWriter* rw) const
{
	bool result = this->MetricBaseMultiplayerMissionDone::Write(rw);

	result = result && this->MetricPlayerCoords::Write(rw);

	if (m_WeaponHash > 0)
	{
		result = result && rw->WriteUns("w", m_WeaponHash);
	}

	if (m_OtherItem > 0)
	{
		result = result && rw->WriteUns("o", m_OtherItem);
	}

	if (m_EnemiesKilled > 0)
	{
		result = result && rw->WriteUns("e", m_EnemiesKilled);
	}

	result = result && rw->BeginArray("itms", NULL);
	{
		for(u32 i=0; i<MetricAMBIENT_MISSION_CRATEDROP::MAX_CRATE_ITEM_HASHS; i++)
		{
			result = result && rw->WriteUns(NULL, m_SpecialItemHashs[i]);
		}
	}
	result = result && rw->End();

	if (m_CollectedBodyArmour)
	{
		result = result && rw->WriteBool("ba", m_CollectedBodyArmour);
	}

	return result;
}

/// MetricAMBIENT_MISSION_CRATE_DESTROY ---------------------------------------------------

MetricAMBIENT_MISSION_CRATE_CREATED::MetricAMBIENT_MISSION_CRATE_CREATED(float X, float Y, float Z)
	: MetricPlayStat()
	, m_X(X)
	, m_Y(Y)
	, m_Z(Z)
{

}

bool MetricAMBIENT_MISSION_CRATE_CREATED::Write(RsonWriter* rw) const
{
	bool result = this->MetricPlayStat::Write(rw);

	result = result && rw->BeginArray("c", NULL);
	{
		result = result && rw->WriteFloat(NULL, m_X, "%.2f");
		result = result && rw->WriteFloat(NULL, m_Y, "%.2f");
		result = result && rw->WriteFloat(NULL, m_Z, "%.2f");
	}
	result = result && rw->End();

	return result;
}

/// MetricAMBIENT_MISSION_RACE_TO_POINT ---------------------------------------------------

MetricAMBIENT_MISSION_RACE_TO_POINT::MetricAMBIENT_MISSION_RACE_TO_POINT(const u32  missionId, const u32 xpEarned, const u32 cashEarned, const sRaceToPointInfo& rtopInfo) 
	: MetricBaseMultiplayerMissionDone(missionId, xpEarned, cashEarned, 0)
	, m_LocationHash(rtopInfo.m_LocationHash)
	, m_MatchId(rtopInfo.m_MatchId)
	, m_NumPlayers(rtopInfo.m_NumPlayers)
	, m_RaceWon(rtopInfo.m_RaceWon)
	, m_StartX(0.0f)
	, m_StartY(0.0f)
	, m_StartZ(0.0f)
	, m_EndX(0.0f)
	, m_EndY(0.0f)
	, m_EndZ(0.0f)
{
	m_StartX = rtopInfo.m_RaceStartPos.x;
	m_StartY = rtopInfo.m_RaceStartPos.y;
	m_StartZ = rtopInfo.m_RaceStartPos.z;

	m_EndX   = rtopInfo.m_RaceEndPos.x;
	m_EndY   = rtopInfo.m_RaceEndPos.y;
	m_EndZ   = rtopInfo.m_RaceEndPos.z;
}

bool MetricAMBIENT_MISSION_RACE_TO_POINT::Write(RsonWriter* rw) const
{
	bool result = this->MetricBaseMultiplayerMissionDone::Write(rw);

	result = result && rw->WriteUns("l", m_LocationHash);
	result = result && rw->WriteUns("m", m_MatchId);
	result = result && rw->WriteUns("n", m_NumPlayers);

	result = result && rw->WriteBool("w", m_RaceWon);

	result = result && rw->BeginArray("sc", NULL);
	{
		result = result && rw->WriteFloat(NULL, m_StartX, "%.2f");
		result = result && rw->WriteFloat(NULL, m_StartY, "%.2f");
		result = result && rw->WriteFloat(NULL, m_StartZ, "%.2f");
	}
	result = result && rw->End();

	result = result && rw->BeginArray("ec", NULL);
	{
		result = result && rw->WriteFloat(NULL, m_EndX, "%.2f");
		result = result && rw->WriteFloat(NULL, m_EndY, "%.2f");
		result = result && rw->WriteFloat(NULL, m_EndZ, "%.2f");
	}
	result = result && rw->End();

	return result;
}

/// MetricFREEMODE_CREATOR_USAGE ---------------------------------------------------

MetricFREEMODE_CREATOR_USAGE::MetricFREEMODE_CREATOR_USAGE(u32 timeSpent, u32 numberOfTries, u32 numberOfEarlyQuits, bool isCreating, u32 missionId)
	: MetricPlayStat()
	, m_timeSpent(timeSpent)
	, m_numberOfTries(numberOfTries)
	, m_numberOfEarlyQuits(numberOfEarlyQuits)
	, m_isCreating(isCreating)
	, m_missionId(missionId)
{

}

bool MetricFREEMODE_CREATOR_USAGE::Write(RsonWriter* rw) const
{
	bool result = this->MetricPlayStat::Write(rw);

	result = result && rw->WriteUns("t", m_timeSpent);
	result = result && rw->WriteUns("a", m_numberOfTries);
	result = result && rw->WriteUns("q", m_numberOfEarlyQuits);
	result = result && rw->WriteBool("c", m_isCreating);
	result = result && rw->WriteUns("m", m_missionId);

	return result;
}

/// MetricPURCHASE_FLOW ---------------------------------------------------


MetricPURCHASE_FLOW::PurchaseMetricInfos::PurchaseMetricInfos()
{
	Reset();
}

void MetricPURCHASE_FLOW::PurchaseMetricInfos::SaveCurrentCash()
{
	if(!StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT) 
	&& !StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT) 
	&& CLiveManager::GetProfileStatsMgr().GetMPSynchIsDone()) // Make sure the values below are loaded
	{
		m_bank      = MoneyInterface::GetVCBankBalance();
		m_evc       = MoneyInterface::GetEVCBalance();
		m_pvc       = MoneyInterface::GetPVCBalance();
		m_pxr       = MoneyInterface::GetPXRValue();
		m_slot      = StatsInterface::GetCurrentMultiplayerCharaterSlot();
		for(int i=0; i<MAX_NUM_MP_CHARS; i++)
		{
			m_charactersCash[i] = MoneyInterface:: GetVCWalletBalance (i);
		}
	}
}

void MetricPURCHASE_FLOW::PurchaseMetricInfos::Reset()
{
	m_fromLocation=SPL_UNKNOWN;
	m_reservedLocation=SPL_UNKNOWN;
	m_storeId=0;
	m_lastItemViewed=0;
	m_lastItemViewedPrice=0;
	m_bank=0;
	m_evc=0;
	m_pvc=0; 
	m_pxr=0; 

	for(int i=0; i<MAX_NUM_MP_CHARS; i++)
	{
		m_charactersCash[i] = 0;
	}
	m_slot = 0;
	m_currentProductIdCount=0;
}

void MetricPURCHASE_FLOW::PurchaseMetricInfos::AddProductId(const atString& productId)
{
	if(gnetVerifyf(m_currentProductIdCount<MAX_PRODUCT_IDS, "Too many products bought during the same Store Open") && 
		gnetVerifyf(productId.GetLength() <= MAX_PRODUCT_ID_LENGTH, "Product ID '%s' exceeds maximum length %d", productId.c_str(), MAX_PRODUCT_ID_LENGTH))
	{
		strcpy_s(m_productIds[m_currentProductIdCount], MAX_PRODUCT_ID_LENGTH, productId.c_str());
		m_currentProductIdCount++;
	}
}

void MetricPURCHASE_FLOW::PurchaseMetricInfos::SetFromLocation(const eStorePurchaseLocation location)
{
	gnetDebug1("PurchaseMetricInfos - SetFromLocation %s - ReservedLocation %s", GetPurchaseLocationHash(location).TryGetCStr(), GetPurchaseLocationHash(m_reservedLocation).TryGetCStr());

	if (m_reservedLocation == SPL_UNKNOWN)
	{
		m_fromLocation = location;
	}
	else
	{
		m_fromLocation = m_reservedLocation;
	}

	m_reservedLocation = SPL_UNKNOWN;
}

void MetricPURCHASE_FLOW::PurchaseMetricInfos::ReserveFromLocation(const eStorePurchaseLocation location)
{
	gnetDebug1("PurchaseMetricInfos - ReserveFromLocation %s", GetPurchaseLocationHash(location).TryGetCStr());
	m_reservedLocation = location;
}

atHashString GetPurchaseLocationHash(const eStorePurchaseLocation location)
{
	switch (location)
	{
	case SPL_UNKNOWN: return ATSTRINGHASH("SPL_UNKNOWN", 0x60540AE3);
	case SPL_SCUI: return ATSTRINGHASH("SPL_SCUI", 0x3909171A);
	case SPL_INGAME: return ATSTRINGHASH("SPL_INGAME", 0x20B204D9);
	case SPL_STORE: return ATSTRINGHASH("SPL_STORE", 0x51C2BC1C);
	case SPL_PHONE: return ATSTRINGHASH("SPL_PHONE", 0xA2A83DF);
	case SPL_AMBIENT: return ATSTRINGHASH("SPL_AMBIENT", 0xEF1FC19D);
	case SPL_ONLINE_TAB: return ATSTRINGHASH("SPL_ONLINE_TAB", 0x3FAC0122);
	case SPL_STORE_GUN: return ATSTRINGHASH("SPL_STORE_GUN", 0x23A65582);
	case SPL_STORE_CLOTH: return ATSTRINGHASH("SPL_STORE_CLOTH", 0x5380090);
	case SPL_STORE_HAIR: return ATSTRINGHASH("SPL_STORE_HAIR", 0xD04444C1);
	case SPL_STORE_CARMOD: return ATSTRINGHASH("SPL_STORE_CARMOD", 0xA6E404DD);
	case SPL_STORE_TATTOO: return ATSTRINGHASH("SPL_STORE_TATTOO", 0xA8448D88);
	case SPL_STORE_PERSONAL_MOD: return ATSTRINGHASH("SPL_STORE_PERSONAL_MOD", 0x5A50EA6E);
	case SPL_STARTER_PACK: return ATSTRINGHASH("SPL_STARTER_PACK", 0x7A4275E7);
	case SPL_LANDING_MP: return ATSTRINGHASH("SPL_LANDING_MP", 0x56CF8FAE);
	case SPL_LANDING_SP_UPSELL: return ATSTRINGHASH("SPL_LANDING_SP_UPSELL", 0x22D38824);
	case SPL_PRIORITY_FEED: return ATSTRINGHASH("SPL_PRIORITY_FEED", 0x2B5052B0);
	case SPL_WHATS_NEW: return ATSTRINGHASH("SPL_WHATS_NEW", 0x13984433);
	case SPL_MEMBERSHIP_INGAME: return ATSTRINGHASH("SPL_MEMBERSHIP_INGAME", 0x62F52CBD);
	case SPL_PAUSE_PLAY_SP: return ATSTRINGHASH("SPL_PAUSE_PLAY_SP", 0xC745291C);
	case SPL_CHARACTER_SELECTION_WHEEL: return ATSTRINGHASH("SPL_CHARACTER_SELECTION_WHEEL", 0x845564B);
	}

	gnetAssertf(false, "Unhandled value");
	return ATSTRINGHASH("SPL_UNKNOWN", 0x60540AE3);
}

MetricPURCHASE_FLOW::MetricPURCHASE_FLOW()
	: MetricPlayStat()
	, m_metricInformation(CNetworkTelemetry::GetPurchaseMetricInformation())
{

}


bool MetricPURCHASE_FLOW::Write(RsonWriter* rw) const
{
	bool result = this->MetricPlayStat::Write(rw);

	result = result && rw->WriteUns("l", m_metricInformation.GetFromLocation());
	result = result && rw->WriteUns("sid", m_metricInformation.m_storeId);
	result = result && rw->WriteUns("liid", m_metricInformation.m_lastItemViewed);
	result = result && rw->WriteUns("lip", m_metricInformation.m_lastItemViewedPrice);
	result = result && rw->WriteInt64("b", m_metricInformation.m_bank);
	result = result && rw->WriteInt64("evc", m_metricInformation.m_evc);
	result = result && rw->WriteInt64("pvc", m_metricInformation.m_pvc);
	result = result && rw->WriteFloat("pxr", m_metricInformation.m_pxr);
	result = result && rw->WriteInt("ch", m_metricInformation.m_slot);
	result = result && rw->BeginArray("c", NULL);
	for(int i=0; i< MAX_NUM_MP_CHARS; i++)
	{
		result = result && rw->WriteInt64(NULL, m_metricInformation.m_charactersCash[i]);
	}
	result = result && rw->End();
	result = result && rw->BeginArray("pids", NULL);
	for(int i=0; i<m_metricInformation.m_currentProductIdCount; i++)
	{
		result = result && rw->WriteString(NULL, m_metricInformation.m_productIds[i]);
	}
	result = result && rw->End();

	return result;
}

/// MetricXP_LOSS ---------------------------------------------------

rage::atString* MetricXP_LOSS::sm_callstack=NULL;

MetricXP_LOSS::MetricXP_LOSS(int previousXp, int newXp)
	: MetricPlayStat()
	, m_PreviousXp(previousXp)
	, m_NewXp(newXp)
{
	for(int i=0; i<MAX_CALLSTACK_SIZE; i++)
	{
		sysMemSet( (void*) &m_CallstackFrames[i] , 0, MAX_CALLSTACKFRAME_SIZE);
	}
	// Get script callstack
	atString callstack;
	scrThread *pActiveThread = scrThread::GetActiveThread();
	if (gnetVerifyf(pActiveThread, "No active script thread when the XP was decreased"))
	{
		sm_callstack = &callstack;
		pActiveThread->PrintStackTrace(AppendCallstack);
		sm_callstack = NULL;
		atArray<atString> split;
		callstack.Split(split, "\n", true);

		int i=0; 
		while(i<MAX_CALLSTACK_SIZE && i<split.size())
		{
			split[i].Replace(">>>", "");
			if(stristr(split[i].c_str(), "+"))
				split[i].Truncate( (int) (stristr(split[i].c_str(), "+")-split[i].c_str()) );  // truncate after the first occurence of '+'
			strcpy_s(m_CallstackFrames[i], MAX_CALLSTACKFRAME_SIZE, split[i]);
			i++;
		}
	}
}

bool MetricXP_LOSS::Write(RsonWriter* rw) const
{
	bool result = this->MetricPlayStat::Write(rw);

	result = result && rw->WriteUns("p", m_PreviousXp);
	result = result && rw->WriteUns("n", m_NewXp);
	result = result && rw->BeginArray("c", NULL);
	for(int i=0; i<MAX_CALLSTACK_SIZE; i++)
	{
		if(strlen(m_CallstackFrames[i])>0)
		result = result && rw->WriteString(NULL, m_CallstackFrames[i]);
	}
	result = result && rw->End();

	return result;
}

void MetricXP_LOSS::AppendCallstack(const char* fmt, ...)
{
	if(!gnetVerifyf(sm_callstack, "sm_callstack should be assigned to a valid atString pointer"))
		return;

	va_list args;
	va_start(args, fmt);

	char buffer[1024];
	vsnprintf(buffer, sizeof(buffer), fmt, args);

	*sm_callstack += buffer;
	*sm_callstack += '\n';

	va_end(args);
}

/// MetricPURCHASE_FLOW ---------------------------------------------------

MetricHEIST_SAVE_CHEAT::MetricHEIST_SAVE_CHEAT(int hashValue, int otherValue)
	: MetricPlayStat()
	, m_hashValue(hashValue)
	, m_otherHashValue(otherValue)
{

}

bool MetricHEIST_SAVE_CHEAT::Write(RsonWriter* rw) const
{
	bool result = this->MetricPlayStat::Write(rw);
	result = result && rw->WriteUns("v", m_hashValue);
	result = result && rw->WriteUns("w", m_otherHashValue);
	return result;
}

/// MetricDIRECTOR_MODE ---------------------------------------------------

MetricDIRECTOR_MODE::MetricDIRECTOR_MODE(const VideoClipMetrics& values)
	: MetricPlayStat()
	, m_characterHash(values.m_characterHash)
	, m_timeOfDay(values.m_timeOfDay)
	, m_weather(values.m_weather)
	, m_wantedLevel(values.m_wantedLevel)
	, m_pedDensity(values.m_pedDensity)
	, m_vehicleDensity(values.m_vehicleDensity)
	, m_restrictedArea(values.m_restrictedArea)
	, m_invulnerability(values.m_invulnerability)
{

}

bool MetricDIRECTOR_MODE::Write(RsonWriter* rw) const
{
	bool result = this->MetricPlayStat::Write(rw);
	result = result && rw->WriteUns("ch", m_characterHash);
	result = result && rw->WriteUns("t", m_timeOfDay);
	result = result && rw->WriteUns("w", m_weather);
	result = result && rw->WriteBool("wl", m_wantedLevel);
	result = result && rw->WriteUns("pd", m_pedDensity);
	result = result && rw->WriteUns("vd", m_vehicleDensity);
	result = result && rw->WriteBool("ra", m_restrictedArea);
	result = result && rw->WriteBool("i", m_invulnerability);
	return result;
}

/// MetricVIDEO_EDITOR_SAVE ---------------------------------------------------

MetricVIDEO_EDITOR_SAVE::MetricVIDEO_EDITOR_SAVE(int clipCount, int* audioHash, int audioHashCount, u64 projectLength, u64 timeSpent)
	: MetricPlayStat()
	, m_clipCount(clipCount)
	, m_usedAudioHash(audioHashCount)
	, m_projectLength(projectLength)
	, m_timeSpent(timeSpent)
{
	memset(m_audioHash, 0, MAX_AUDIO_HASH*sizeof(int));
	for(int i=0; i<audioHashCount && i<MAX_AUDIO_HASH; i++)
	{
		m_audioHash[i]=audioHash[i];
	}
}


bool MetricVIDEO_EDITOR_SAVE::Write(RsonWriter* rw) const
{
	bool result = this->MetricPlayStat::Write(rw);

	result = result && rw->WriteUns("c", m_clipCount);
	result = result && rw->WriteInt64("l", m_projectLength);
	result = result && rw->WriteInt64("t", m_timeSpent);
	result = result && rw->BeginArray("a", NULL);
	for(int i=0; i<MAX_AUDIO_HASH && m_audioHash[i]!=0; i++)
	{
		result = result && rw->WriteUns(NULL, m_audioHash[i]);
	}
	result = result && rw->End();
	result = result && rw->WriteUns("m", m_usedAudioHash);

	return result;
}

/// MetricVIDEO_EDITOR_UPLOAD ---------------------------------------------------

MetricVIDEO_EDITOR_UPLOAD::MetricVIDEO_EDITOR_UPLOAD(u64 videoLength, int scScore)
	: MetricPlayStat()
	, m_videoLength(videoLength)
	, m_scScore(scScore)
{

}


bool MetricVIDEO_EDITOR_UPLOAD::Write(RsonWriter* rw) const
{
	bool result = this->MetricPlayStat::Write(rw);

	result = result && rw->WriteInt64("l", m_videoLength);
	result = result && rw->WriteUns("s", m_scScore);

	return result;
}

#if RSG_PC

/// MetricBENCHMARK_FPS ---------------------------------------------------

MetricBENCHMARK_FPS::MetricBENCHMARK_FPS(const atArray<EndUserBenchmarks::fpsResult>& values)
	: MetricPlayStat()
	, m_passCount(0)
{
	for(int i=0;i<values.size() && i<MAX_PASS_COUNT;i++)
	{
		const EndUserBenchmarks::fpsResult &thisResult = values[i];

		m_fpsValues[i][0]=thisResult.min;
		m_fpsValues[i][1]=thisResult.max;
		m_fpsValues[i][2]=thisResult.average;

		int frameCount=0;
		for(int frameTime=0; frameTime < 16; ++frameTime)
		{
			frameCount += thisResult.frameTimeSlots[frameTime];
		}
		m_under16ms[i]=((float)frameCount/(float)thisResult.frameCount)*100.0f;

		frameCount=0;
		for(int frameTime=0; frameTime < 33; ++frameTime)
		{
			frameCount += thisResult.frameTimeSlots[frameTime];
		}
		m_under33ms[i]=((float)frameCount/(float)thisResult.frameCount)*100.0f;

		m_passCount++;
	}
}


bool MetricBENCHMARK_FPS::Write(RsonWriter* rw) const
{
	bool result = this->MetricPlayStat::Write(rw);

	result = result && rw->WriteUns64("uid", CNetworkTelemetry::GetBenchmarkGuid());
	
	// Fps
	result = result && rw->BeginArray("fps", NULL);
	for(int i=0;i<m_passCount;i++)
	{
		result = result && rw->Begin(NULL, NULL);
		result = result && rw->WriteFloat("min", m_fpsValues[i][0], "%.2f");
		result = result && rw->WriteFloat("max", m_fpsValues[i][1], "%.2f");
		result = result && rw->WriteFloat("avg", m_fpsValues[i][2], "%.2f");
		result = result && rw->End();
	}
	result = result && rw->End();

	// Frames under 16ms
	result = result && rw->BeginArray("u16", NULL);
	for(int i=0;i<m_passCount;i++)
	{
		result = result && rw->WriteFloat(NULL, m_under16ms[i], "%.2f");
	}
	result = result && rw->End();

	// Frames under 33ms
	result = result && rw->BeginArray("u33", NULL);
	for(int i=0;i<m_passCount;i++)
	{
		result = result && rw->WriteFloat(NULL, m_under33ms[i], "%.2f");
	}
	result = result && rw->End();

	return result;
}

/// MetricBENCHMARK_P ---------------------------------------------------

MetricBENCHMARK_P::MetricBENCHMARK_P(const atArray<EndUserBenchmarks::fpsResult>& values)
	: MetricPlayStat()
	, m_passCount(0)
{
	int percentiles[] = { 50, 75, 80, 85, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99 };

	for(int i=0; i<values.size() && i<MAX_PASS_COUNT; i++)
	{
		const EndUserBenchmarks::fpsResult& thisResult = values[i];

		for(int percentileNum=0; percentileNum<NUM_BENCHMARK_PERCENTILES; ++percentileNum)
		{
			// Count the number of frames inside the 'n'th percentile.
			int sampleCountRequiredForPercentile = (int)((float)(thisResult.frameCount / (100.0f/(float)percentiles[percentileNum])));
			int sampleCountUnderPercentile=0;
			int numMs;
			for(numMs=0; numMs<MAX_FRAME_TIME_MS; ++numMs)
			{
				if(sampleCountUnderPercentile + thisResult.frameTimeSlots[numMs] >= sampleCountRequiredForPercentile)
					break;
				sampleCountUnderPercentile += thisResult.frameTimeSlots[numMs];
				m_percentiles[i][percentileNum] = numMs;
			}
		}

		m_passCount++;
	}
}


bool MetricBENCHMARK_P::Write(RsonWriter* rw) const
{
	bool result = this->MetricPlayStat::Write(rw);

	result = result && rw->WriteUns64("uid", CNetworkTelemetry::GetBenchmarkGuid());

	// Percentiles
	result = result && rw->BeginArray("p", NULL);
	for(int i=0;i<m_passCount;i++)
	{
		result = result && rw->Begin(NULL, NULL);
		result = result && rw->WriteInt("id", i);
		result = result && rw->BeginArray("d", NULL);
		for(int percentileNum=0; percentileNum<NUM_BENCHMARK_PERCENTILES; ++percentileNum)
		{
			result = result && rw->WriteInt(NULL, m_percentiles[i][percentileNum]);
		}
		result = result && rw->End();
		result = result && rw->End();
	}
	result = result && rw->End();

	return result;
}
#endif

/// MetricCASH_CREATED ---------------------------------------------------

MetricCASH_CREATED::MetricCASH_CREATED(u64 uid, const rlGamerHandle& player, int amount, int hash)
	: MetricPlayStat()
	, m_uid(uid)
	, m_amount(amount)
	, m_hash(hash)
{
	m_GamerHandle[0] = '\0';
	if (gnetVerify(player.IsValid()))
	{
		player.ToString(m_GamerHandle, COUNTOF(m_GamerHandle));
	}
}

bool MetricCASH_CREATED::Write(RsonWriter* rw) const
{
	bool result = this->MetricPlayStat::Write(rw);

	result = result && rw->WriteInt64("id", m_uid);
	if (gnetVerify(m_GamerHandle[0] != '\0'))
	{
		result = result && rw->WriteString("g", m_GamerHandle);
	}
	result = result && rw->WriteInt("a", m_amount);
	result = result && rw->WriteInt("h", m_hash);

	return result;
}


/// MetricFAILED_YOUTUBE_SUB ---------------------------------------------------

MetricFAILED_YOUTUBE_SUB::MetricFAILED_YOUTUBE_SUB(int _errorCode, const char* errorString, const char* uiFlowState)
	: MetricPlayStat()
	, m_errorCode(_errorCode)
{
	sysMemSet( (void*) m_errorString , 0, MAX_ERROR_STRING_SIZE);
	sysMemSet( (void*) m_uiFlowState , 0, MAX_UI_FLOW_STATE_SIZE);
	
	if(errorString)
		safecpy(m_errorString, errorString, MAX_ERROR_STRING_SIZE);
	if(uiFlowState)
		safecpy(m_uiFlowState, uiFlowState, MAX_UI_FLOW_STATE_SIZE);
}

bool MetricFAILED_YOUTUBE_SUB::Write(RsonWriter* rw) const
{
	bool result = this->MetricPlayStat::Write(rw);

	result = result && rw->WriteInt("e", m_errorCode);
	result = result && rw->WriteString("es", m_errorString);
	result = result && rw->WriteString("fs", m_uiFlowState);

	return result;
}


/// MetricSTARTERPACK_APPLIED ---------------------------------------------------

MetricSTARTERPACK_APPLIED::MetricSTARTERPACK_APPLIED(int item, int amount)
	: MetricPlayStat()
	, m_item(item)
	, m_amount(amount)
{

}

bool MetricSTARTERPACK_APPLIED::Write(RsonWriter* rw) const
{
	bool result = this->MetricPlayStat::Write(rw);

	result = result && rw->WriteInt("i", m_item);
	result = result && rw->WriteInt("a", m_amount);

	return result;
}

/// MetricLAST_VEH ---------------------------------------------------

static_assert(MetricLAST_VEH::NUM_FIELDS < 26, "Too many fields, change the loop below to generate the correct field names in the json");

MetricLAST_VEH::MetricLAST_VEH()
{
	for(int i=0; i<NUM_FIELDS; i++)
		m_fields[i] = fwRandom::GetRandomNumber();

	m_FromGH[0] = '\0';
	m_ToGH[0] = '\0';

	m_time = rlGetPosixTime();
	m_session = CNetwork::GetNetworkSession().GetSnSession().GetSessionToken().m_Value;
}

bool MetricLAST_VEH::Write(RsonWriter* rw) const
{
	bool result = this->MetricPlayStat::Write(rw);

	char fieldName[2];
	for(int i=0; i<NUM_FIELDS; i++)
	{
		formatf(fieldName, "%c", 'a'+i);  // Start at 'a', increment
		result = result && rw->WriteInt(fieldName, m_fields[i]);
	}

	result = result && rw->WriteInt64("ti", m_time);
	result = result && rw->WriteInt64("se", m_session);
	result = result && rw->WriteString("fgh", m_FromGH);
	result = result && rw->WriteString("tgh", m_ToGH);

	return result;
}

/// MetricFIRST_VEH ---------------------------------------------------

MetricFIRST_VEH::MetricFIRST_VEH(int eventType, int ignoredEventCount)
	: m_eventType(eventType)
	, m_ignoredEventCount(ignoredEventCount)
	, m_inSessionGamerhandles(0)
{
	memset(m_gamerHandle, 0, sizeof(char) * RL_MAX_GAMER_HANDLE_CHARS);
	const rlGamerHandle& gh = NetworkInterface::GetLocalGamerHandle();
	gh.ToString(m_gamerHandle, COUNTOF(m_gamerHandle));

	memset(m_playersInSession, 0, sizeof(char) * RL_MAX_GAMER_HANDLE_CHARS * MAX_NUM_PHYSICAL_PLAYERS);
	rlGamerInfo aGamerInfo[MAX_NUM_PHYSICAL_PLAYERS];
	unsigned nGamers = CNetwork::GetNetworkSession().GetSessionMembers(aGamerInfo, MAX_NUM_PHYSICAL_PLAYERS);
	for(int i=0; i<nGamers; i++)
	{
		if (gnetVerify(aGamerInfo[i].GetGamerHandle().IsValid()) && !aGamerInfo[i].GetGamerHandle().IsLocal())
		{
			aGamerInfo[i].GetGamerHandle().ToString(m_playersInSession[m_inSessionGamerhandles], COUNTOF(m_playersInSession[m_inSessionGamerhandles]));
			m_inSessionGamerhandles++;
		}
	}
}

bool MetricFIRST_VEH::Write(RsonWriter* rw) const
{
	bool result = this->MetricPlayStat::Write(rw);

	result = result && rw->WriteInt("id", m_eventType);
	result = result && rw->WriteInt("c", m_ignoredEventCount);
	result = result && rw->WriteString("p", m_gamerHandle);
	result = result && rw->BeginArray("g", NULL);
	{
		for(int i=0; i<m_inSessionGamerhandles; i++)
		{
			result = result && rw->WriteString(NULL, m_playersInSession[i]);
		}
	}
	rw->End();
	
	return result;
}

MetricOBJECT_REASSIGN_FAIL::MetricOBJECT_REASSIGN_FAIL(const int playerCount, const int restartCount, const int ambObjCount, const int scrObjCount)
{
	m_playerCount = playerCount;
	m_restartCount = restartCount;
	m_ambObjCount = ambObjCount;
	m_scrObjCount = scrObjCount;
}

bool MetricOBJECT_REASSIGN_FAIL::Write(RsonWriter* rw) const
{
	bool result = MetricPlayStat::Write(rw);
	result &= rw->WriteInt("a", m_playerCount);
	result &= rw->WriteInt("b", m_restartCount);
	result &= rw->WriteInt("c", m_ambObjCount);
	result &= rw->WriteInt("d", m_scrObjCount);
	return result;
}

MetricOBJECT_REASSIGN_INFO::MetricOBJECT_REASSIGN_INFO(const int playersInvolved, const int reassignCount, const float restartAvg, const float timeAvg, const int maxDuration)
{
	m_playersInvolved = playersInvolved;
	m_reassignCount = reassignCount;
	m_restartAvg = restartAvg;
	m_timeAvg = timeAvg;
	m_maxDuration = maxDuration;
}

bool MetricOBJECT_REASSIGN_INFO::Write(RsonWriter* rw) const
{
	bool result = MetricPlayStat::Write(rw);
	result &= rw->WriteInt("a", m_reassignCount);
	result &= rw->WriteFloat("b", m_restartAvg);
	result &= rw->WriteFloat("c", m_timeAvg);
	result &= rw->WriteInt("d", m_maxDuration);
	result &= rw->WriteInt("e", m_playersInvolved);
	return result;
}

MetricUGC_NAV::MetricUGC_NAV(const int feature, const int location)
	: m_feature(feature)
	, m_location(location)
{

}

bool MetricUGC_NAV::Write(RsonWriter* rw) const
{
	bool result = MetricPlayStat::Write(rw);
	result &= rw->WriteInt("f", m_feature);
	result &= rw->WriteInt("l", m_location);
	return result;
}

/// MetricBandwidthTest ---------------------------------------------------
MetricBandwidthTest::MetricBandwidthTest(const u64 uploadbps, const u64 downloadbps, const unsigned source, const int resultCode)
	: m_Uploadbps(uploadbps)
	, m_Downloadbps(downloadbps)
	, m_Source(source)
	, m_ResultCode(resultCode)
	, m_Region(-1)
{
	m_CountryCode[0] = '\0';

	rlRosGeoLocInfo geolocInfo;
	if (rlRos::GetGeoLocInfo(&geolocInfo))
	{
		safecpy(m_CountryCode, geolocInfo.m_CountryCode);
		m_Region = geolocInfo.m_RegionCode;
	}
}

bool MetricBandwidthTest::Write(RsonWriter* rw) const
{
	return rlMetric::Write(rw)
		&& rw->WriteUns64("u", m_Uploadbps)
		&& rw->WriteUns64("d", m_Downloadbps)
		&& rw->WriteUns("s", m_Source)
		&& rw->WriteInt("r", m_ResultCode)
		&& rw->WriteInt("g", m_Region)
		&& rw->WriteString("c", m_CountryCode);
}

#if RSG_GEN9
/// MetricPlatformInfos ---------------------------------------------------

MetricPlatformInfos::MetricPlatformInfos()
	: rlMetric()
	, m_hardware(sga::HW_UNKNOWN)
{
	if (sga::AdapterManager::GetInstance())
	{
		m_hardware = sga::AdapterManager::GetInstance()->GetHardware();
	}
}


bool MetricPlatformInfos::Write(RsonWriter* rw) const
{
	bool result = this->rlMetric::Write(rw);
	result = result && rw->WriteInt("h", (int)m_hardware);
	return result;
}
#endif // RSG_GEN9

MetricSP_SAVE_MIGRATION::MetricSP_SAVE_MIGRATION(bool success, u32 savePosixTime, float completionPercentage, u32 lastCompletedMissionId, unsigned sizeOfSavegameDataBuffer)
	: m_success(success)
	, m_savePosixTime(savePosixTime)
	, m_completionPercentage(completionPercentage)
	, m_lastCompletedMissionId(lastCompletedMissionId)
	, m_sizeOfSavegameDataBuffer(sizeOfSavegameDataBuffer)
{

}

bool MetricSP_SAVE_MIGRATION::Write(RsonWriter* rw) const
{
	return rlMetric::Write(rw)
		&& rw->WriteBool("s", m_success)
		&& rw->WriteUns("t", m_savePosixTime)
		&& rw->WriteFloat("p", m_completionPercentage)
		&& rw->WriteUns("l", m_lastCompletedMissionId)
		&& rw->WriteUns("bs", m_sizeOfSavegameDataBuffer);
}

MetricEventTriggerOveruse::MetricEventTriggerOveruse(Vector3 pos, u32 rank, u32 triggerCount, u32 triggerDur, u32 totalCount,  atFixedArray<EventData, MAX_EVENTS_REPORTED>& triggeredData, int sType, int mType, int contentId)
	: m_playerPos(pos)
	, m_playerRank(rank)
	, m_minTriggerCount(triggerCount)
	, m_triggerDuration(triggerDur)
	, m_totalEventCount(totalCount)
	, m_triggeredEventsData(triggeredData)
	, m_sessionType(sType)
	, m_missionType(mType)
	, m_rootContentId(contentId)
{
}

bool MetricEventTriggerOveruse::Write(RsonWriter* rw) const
{
	u32 triggeredEventTypes[MAX_EVENTS_REPORTED];
	int triggeredEventCount[MAX_EVENTS_REPORTED];
	int count = MAX_EVENTS_REPORTED;
	for(int i = 0; i < MAX_EVENTS_REPORTED; i++)
	{
		if(i < m_triggeredEventsData.GetCount())
		{
			if(m_triggeredEventsData[i].m_triggerCount == 0)
			{
				count = i;
				break;
			}
			triggeredEventTypes[i] = m_triggeredEventsData[i].m_eventHash;
			triggeredEventCount[i] = m_triggeredEventsData[i].m_triggerCount;
		}
		else
		{
			count = i;
			break;
		}
	}

	bool result = rlMetric::Write(rw);
	result &= rw->BeginArray("pc", NULL);
	{
		result &= rw->WriteFloat(NULL, m_playerPos.x, "%.2f");
		result &= rw->WriteFloat(NULL, m_playerPos.y, "%.2f");
		result &= rw->WriteFloat(NULL, m_playerPos.z, "%.2f");
	}
	result &= rw->End();
	
	result &= rw->WriteUns("ra", m_playerRank);
	result &= rw->WriteUns("t", m_totalEventCount);

	result &= rw->BeginArray("netEv", NULL);
	{
		for(int i = 0; i < count; i++)
		{
			result &= rw->Begin(NULL, NULL);
			{
				result &= rw->WriteInt("a", triggeredEventTypes[i]);
				result &= rw->WriteInt("b", triggeredEventCount[i]);
			}
			result &= rw->End();
		}
	}
	result &= rw->End();

	result &= rw->WriteUns("s", m_sessionType);
	result &= rw->WriteUns("m", m_missionType);
	result &= rw->WriteUns("r", m_rootContentId);
	result &= rw->WriteUns("tc", m_minTriggerCount);
	result &= rw->WriteUns("td", m_triggerDuration);
	return result;
}

MetricComms::MetricComms( eChatTelemetryChatCategory chatCateg, int chatType, u32 chatID, u64 sessionID, int sessionType, int messageCount, int totalCharCount, u32 duration, bool vChatDisable, int recAccID, const char* publicContentid, bool isInLobby, const rlGamerHandle* recGamerHandle)
	: MetricPlayStat()
	, m_chatCateg(chatCateg)
	, m_chatType(chatType)
	, m_chatID(chatID)
	, m_sessionID(sessionID)
	, m_sessionType(sessionType)
	, m_messageCount(messageCount)
	, m_charCount(totalCharCount)
	, m_duration(duration)
	, m_vChatDisable(vChatDisable)
	, m_recAccID(recAccID)
	, m_isInLobby(isInLobby)
{
	safecpy(m_publicContentId, publicContentid, COUNTOF(m_publicContentId));

	m_rlUserId[0] = '\0';
	if (!recAccID && recGamerHandle && recGamerHandle->IsValid())
	{
		recGamerHandle->ToUserId(m_rlUserId);
	}
}

MetricComms::MetricComms( const MetricComms::CumulativeTextCommsInfo& cumulativeInfo, eChatTelemetryChatCategory chatCateg, int chatType, u32 chatID, u64 sessionID, int sessionType, bool vChatDisable, int recAccID, const char* publicContentId, bool isInLobby, int durationModifier)
	: MetricPlayStat()
	, m_chatCateg(chatCateg)
	, m_chatType(chatType)
	, m_chatID(chatID)
	, m_sessionID(sessionID)
	, m_sessionType(sessionType)
	, m_messageCount(cumulativeInfo.m_messageCount)
	, m_charCount(cumulativeInfo.m_totalMessageSize)
	, m_vChatDisable(vChatDisable)
	, m_recAccID(recAccID)
	, m_isInLobby(isInLobby)
{
	const u32 kMsInSecond = 1000;
	m_duration = ( sysTimer::GetSystemMsTime() - cumulativeInfo.m_FirstChatTime + durationModifier ) / kMsInSecond;
	safecpy(m_publicContentId, publicContentId, COUNTOF(m_publicContentId));
	m_rlUserId[0] = '\0';
}

MetricComms::MetricComms(const MetricComms::CumulativeVoiceCommsInfo& cumulativeInfo, u32 chatID, u64 sessionID, int sessionType, bool vChatDisable, int recAccID, const char* publicContentId, bool isInLobby)
	: MetricPlayStat()
	, m_chatCateg(MetricComms::VOICE_CHAT)
	, m_chatType(cumulativeInfo.m_voiceChatType)
	, m_chatID(chatID)
	, m_sessionID(sessionID)
	, m_sessionType(sessionType)
	, m_messageCount(0)
	, m_charCount(0)
	, m_vChatDisable(vChatDisable)
	, m_recAccID(recAccID)
	, m_isInLobby(isInLobby)
{
	const u32 kMsInSecond = 1000;
	m_duration = cumulativeInfo.m_totalTimeTalking / kMsInSecond;
	safecpy(m_publicContentId, publicContentId, COUNTOF(m_publicContentId));
	m_rlUserId[0] = '\0';
}
		
bool MetricComms::Write(RsonWriter* rw) const
{
	bool result = MetricPlayStat::Write(rw);
	result &= rw->WriteInt("a", m_chatCateg);
	result &= rw->WriteInt("b", m_chatType);
	if ( m_chatID != 0 )
	result &= rw->WriteInt("c", m_chatID);
	result &= rw->WriteUns64("d", m_sessionID);
	result &= rw->WriteInt("e", m_sessionType);
	result &= rw->WriteInt("f", m_messageCount);
	result &= rw->WriteInt("g", m_charCount);
	result &= rw->WriteInt("h", (int)m_duration);
	result &= rw->WriteBool("i", m_vChatDisable);
	result &= rw->WriteInt("j", m_recAccID);
	result &= rw->WriteString("k", m_publicContentId);
	result &= rw->WriteBool("l", m_isInLobby);

	if (m_rlUserId[0] != '\0')
	{
		result &= rw->WriteString("m", m_rlUserId);
	}
	return result;
}

void MetricComms::CumulativeTextCommsInfo::RecordMessage(const char* text)
{
	if (m_FirstChatTime == 0)
	{
		m_FirstChatTime = sysTimer::GetSystemMsTime();
	}

	m_messageCount++;

	m_lastChatTime = sysTimer::GetSystemMsTime();
	m_totalMessageSize += (int)strlen(text);
}

void MetricComms::CumulativeTextCommsInfo::SendTelemetry( MetricComms::eChatTelemetryCumulativeChatCategory chatType, class CNetGamePlayer* pReceiverPlayer /*= nullptr*/, const int durationModifier /*= 0*/ )
{
	bool commsTelemetryKillswitch = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("DISABLE_COMMS_TELEMETRY", 0xC1356D3F), false);

	if ( !commsTelemetryKillswitch )
	{
		MetricComms m(
			*this,
			MetricComms::GAME_CHAT,
			chatType == MetricComms::GAME_CHAT_CUMULATIVE ? MetricComms::CHAT_TYPE_PUBLIC : MetricComms::CHAT_TYPE_TEAM,
			m_CurrentChatId,
			CNetworkTelemetry::GetCurMpSessionId(),
			NetworkInterface::GetSessionTypeForTelemetry(),
			!NetworkInterface::GetVoice().IsEnabled(),
			pReceiverPlayer ? pReceiverPlayer->GetPlayerAccountId() : 0,
			CNetworkTelemetry::GetCurrentPublicContentId(),
			CNetworkTelemetry::IsInLobby(),
			durationModifier );
		CNetworkTelemetry::AppendMetric(m);
	}

	Reset();
}

void MetricComms::CumulativeVoiceCommsInfo::UsingVoice( bool talking )
{
	if (talking != m_talking)
	{
		// we were talking
		if (m_talking)
		{
			m_totalTimeTalking += sysTimer::GetSystemMsTime() - m_lastTimeStartedTalking;
			m_lastTimeStartedTalking = 0;
		}
		else
		{
			m_lastTimeStartedTalking = sysTimer::GetSystemMsTime();
		}

		m_talking = talking;
	}

	if (talking)
	{
		m_lastChatTime = sysTimer::GetSystemMsTime();
	}
}

void MetricComms::CumulativeVoiceCommsInfo::SendTelemetry( class CNetGamePlayer* pReceiverPlayer /*= nullptr*/ )
{
	bool commsTelemetryKillswitch = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("DISABLE_COMMS_TELEMETRY", 0xC1356D3F), false);

	const int minTimeTalking = 20;
	if ( !commsTelemetryKillswitch && m_totalTimeTalking > minTimeTalking )
	{
		MetricComms m(
			*this,
			m_CurrentVoiceChatId,
			CNetworkTelemetry::GetCurMpSessionId(),
			NetworkInterface::GetSessionTypeForTelemetry(),
			!NetworkInterface::GetVoice().IsEnabled(),
			pReceiverPlayer ? pReceiverPlayer->GetPlayerAccountId() : 0,
			CNetworkTelemetry::GetCurrentPublicContentId(),
			CNetworkTelemetry::IsInLobby() );
		CNetworkTelemetry::AppendMetric(m);
	}

	Reset();
}

#if NET_ENABLE_MEMBERSHIP_METRICS
/// MetricScMembership ---------------------------------------------------

MetricScMembership::MetricScMembership()
#if NET_ENABLE_MEMBERSHIP_FUNCTIONALITY
	: m_MembershipTelemetryState(rlScMembershipTelemetryState::State_Unknown_NotRequested)
#else
	: m_MembershipTelemetryState(NET_DUMMY_NOT_MEMBER_STATE)
#endif
	, m_HasMembership(false)
	, m_StartPosix(0)
	, m_ExpiryPosix(0)
{
#if NET_ENABLE_MEMBERSHIP_FUNCTIONALITY
	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex(); 

	m_MembershipTelemetryState = static_cast<int>(rlScMembership::GetMembershipTelemetryState(localGamerIndex));

	if(rlScMembership::HasMembershipData(localGamerIndex))
	{
		const rlScMembershipInfo& info = rlScMembership::GetMembershipInfo(localGamerIndex);
		m_HasMembership = info.HasMembership();
		m_StartPosix = info.GetStartPosix();
		m_ExpiryPosix = info.GetExpirePosix();
	}
#endif
}

bool MetricScMembership::Write(RsonWriter* rw) const
{
	return rlMetric::Write(rw)
		&& rw->WriteInt("state", m_MembershipTelemetryState)
		&& rw->WriteBool("m", m_HasMembership)
		&& rw->WriteUns64("s", m_StartPosix)
		&& rw->WriteUns64("e", m_ExpiryPosix);
}
#endif

/// MetricPlayedWith ---------------------------------------------------

MetricPlayedWith::MetricPlayedWith(const PlayedWithDict &gamerHandlesDict, u64 mpSessionId)
{
	m_mpSessionId = mpSessionId;

	PlayedWithDictIter iter = gamerHandlesDict.CreateIterator();
	int numEntries = 0;
	// Iterate over the collection, and write out their gamer handle
	// Only serialize up to MAX_PLAYED_WITH_COUNT items
	// This condition in practice should never be needed, though 
	// its an extra layer of safety for not writing out something unbounded
	iter.Start();
	while(!iter.AtEnd() && numEntries < MAX_PLAYED_WITH_COUNT)
	{
		// Keeping the array as static member since it's a bit on the big side otherwise.
		// Due to when this is triggered this shouldn't be an issue.
		safecpy(m_GamerHandles[numEntries], iter.GetKey().c_str());
		++numEntries;
		iter.Next();
	}

	m_GamerHandlesCount = numEntries;
}

bool MetricPlayedWith::Write(RsonWriter* rw) const
{
	bool result = this->MetricPlayStat::Write(rw);
	result = result && rw->WriteInt64("s", m_mpSessionId);

	bool shouldSendPlayerArray = Tunables::GetInstance().TryAccess(MP_GLOBAL_HASH, ATSTRINGHASH("ENABLE_PLAYED_WITH_PLAYER_ARRAY", 0x19EEFD4F), false);
	atStringBuilder playerOutput;
	if(shouldSendPlayerArray && result)
	{
		for (int i=0; i<m_GamerHandlesCount; i++)
		{
			playerOutput.Append(m_GamerHandles[i]);
			playerOutput.Append("|");
		}
	}
	
	if(playerOutput.Length() == 0)
		return result;

	playerOutput.Truncate((short)(playerOutput.Length()-1));
	result = result && rw->WriteString("pa", playerOutput.ToString());

	return result;
}


/// MetricLandingPage ---------------------------------------------------
u32 MetricLandingPage::sm_LpId = 0;
u32 MetricLandingPage::sm_PreviousLpId = 0;

MetricLandingPage::MetricLandingPage()
    : m_LpId(GetLpId())
{
}

bool MetricLandingPage::Write(RsonWriter* rw) const
{
    return rlMetric::Write(rw)
        && rw->WriteUns("lp", m_LpId);
}

void MetricLandingPage::CreateLpId()
{
    sm_LpId = (unsigned)netRandom::GetIntCrypto();
}

void MetricLandingPage::ClearLpId()
{
    sm_LpId = 0;
}

u32 MetricLandingPage::GetLpId()
{
    return sm_LpId;
}

void MetricLandingPage::StorePreviousLpId()
{
    sm_PreviousLpId = sm_LpId;
}

void MetricLandingPage::ClearPreviousLpId()
{
    sm_PreviousLpId = 0;
}

u32 MetricLandingPage::GetPreviousLpId()
{
    return sm_PreviousLpId;
}

unsigned MetricLandingPage::MapPos(unsigned x, unsigned y)
{
    // 
    // This is as per design from PIA
    //
    //------|-------|-------| ------|
    //      | c0    | c1    | c2    |
    // -----|-------|-------|-------|
    // row0 | bit0  | bit4  | bit8  |
    // row1 | bit1  | bit5  | bit9  |
    // row2 | bit2  | bit6  | bit10 |
    // row3 | bit3  | bit7  | bit11 |

    return x * 4 + y;
}

#if GEN9_LANDING_PAGE_ENABLED
/// MetricBigFeedNav ---------------------------------------------------
MetricBigFeedNav::MetricBigFeedNav(u32 hoverTile, u32 leftBy, u32 clickedTile, u32 durationMs, int skipTimerMs, bool whatsNew)
    : m_LpId(MetricLandingPageMisc::GetLpId())
    , m_HoverTile(hoverTile)
    , m_LeftBy(leftBy)
    , m_ClickedTile(clickedTile)
    , m_DurationMs(durationMs)
    , m_SkipTimerMs(skipTimerMs)
    , m_WhatsNew(whatsNew)
{

}

bool MetricBigFeedNav::Write(RsonWriter* rw) const
{
    return rlMetric::Write(rw)
        && rw->WriteUns("lp", m_LpId)
        && rw->WriteUns("ht", m_HoverTile)
        && rw->WriteUns("lb", m_LeftBy)
        && rw->WriteUns("ct", m_ClickedTile)
        && rw->WriteUns("dt", m_DurationMs)
        && rw->WriteUns("st", m_SkipTimerMs)
        && rw->WriteBool("wn", m_WhatsNew);
}

/// MetricLandingPageMisc ---------------------------------------------------
unsigned MetricLandingPageMisc::sm_TriggerLpId = 0;
PriorityFeedEnterState MetricLandingPageMisc::sm_PriorityBigFeedEnterState = PriorityFeedEnterState::Unknown;

MetricLandingPageMisc::MetricLandingPageMisc()
    : m_BigFeedCount(0)
    , m_PriorityBigFeedCount(0)
    , m_Boot(CLandingPage::WasLaunchedFromBoot())
{
    sysMemSet(m_BigFeed, 0, sizeof(m_BigFeed));
    sysMemSet(m_PriorityBigFeed, 0, sizeof(m_PriorityBigFeed));

    CollectData();
}

void MetricLandingPageMisc::CollectData()
{
    CNetworkNewsStoryMgr& mgr = CNetworkNewsStoryMgr::Get();
    const int numNews = mgr.GetNumStories(NEWS_TYPE_TRANSITION_HASH);

    for (int i = 0; i < numNews && i < MAX_CAROUSEL_POSTS; ++i)
    {
        int idx = mgr.GetIndexFromType(NEWS_TYPE_TRANSITION_HASH, i);
        const CNetworkSCNewsStoryRequest* story = mgr.GetStoryAtIndex(idx);

        if (story != nullptr)
        {
            unsigned trackingId = story->GetTrackingId();

            if (trackingId == 0)
            {
                gnetDebug2("No tracking id for what's new story %s", story->GetStoryKey());
                m_BigFeed[m_BigFeedCount] = ATSTRINGHASH("idmissing", 0x30E36989);
            }
            else
            {
                m_BigFeed[m_BigFeedCount] = trackingId;
            }
            ++m_BigFeedCount;
        }
    }

    if (MetricLandingPageMisc::sm_PriorityBigFeedEnterState == PriorityFeedEnterState::Entered)
    {
        const int numPriority = mgr.GetNumStories(NEWS_TYPE_PRIORITY_HASH);

        for (int i = 0; i < numPriority && i < MAX_PRIORITY_POSTS; ++i)
        {
            int idx = mgr.GetIndexFromType(NEWS_TYPE_PRIORITY_HASH, i);
            const CNetworkSCNewsStoryRequest* story = mgr.GetStoryAtIndex(idx);

            if (story != nullptr)
            {
                unsigned trackingId = story->GetTrackingId();

                if (trackingId == 0)
                {
                    gnetDebug2("No tracking id for priority story %s", story->GetStoryKey());
                    m_PriorityBigFeed[m_PriorityBigFeedCount] = ATSTRINGHASH("idmissing", 0x30E36989);
                }
                else
                {
                    m_PriorityBigFeed[m_PriorityBigFeedCount] = trackingId;
                }
                ++m_PriorityBigFeedCount;
            }
        }

        // Once the data is collected we can reset it again till the user sees the big feed the next time.
        MetricLandingPageMisc::sm_PriorityBigFeedEnterState = PriorityFeedEnterState::InfoCollected;
    }
    else
    {
        gnetDebug2("CollectData for priority big feed skipped. Enter state:%d", (int)MetricLandingPageMisc::sm_PriorityBigFeedEnterState);
    }

    // Nothing membership related yet
}

bool MetricLandingPageMisc::Write(RsonWriter* rw) const
{
    rtry
    {
        rcheckall(MetricLandingPage::Write(rw));
        rcheckall(rw->WriteBool("bo", m_Boot));

        if (m_BigFeedCount > 0)
        {
            rcheckall(rw->WriteArray("bf", m_BigFeed, m_BigFeedCount));
        }

        if (m_PriorityBigFeedCount > 0)
        {
            rcheckall(rw->WriteArray("pf", m_PriorityBigFeed, m_PriorityBigFeedCount));
        }

        return true;
    }
    rcatchall
    {
    }

    return false;
}

bool MetricLandingPageMisc::HasData()
{
    return CNetworkNewsStoryMgr::Get().HasCloudRequestCompleted() && sm_PriorityBigFeedEnterState != PriorityFeedEnterState::Unknown;
}

void MetricLandingPageMisc::SetPriorityBigFeedEnterState(const PriorityFeedEnterState state)
{
    sm_PriorityBigFeedEnterState = state;
}

/// MetricLandingPageNav ---------------------------------------------------
u32 MetricLandingPageNav::sm_Tab = 0;
u32 MetricLandingPageNav::sm_LastActionMs = 0;
u32 MetricLandingPageNav::sm_HoverTiles = 0;

MetricLandingPageNav::MetricLandingPageNav(u32 clickedTile, u32 leftBy, u32 activePage)
    : m_ActivePage(activePage == 0 ? sm_Tab : activePage)
    , m_HoverTiles(sm_HoverTiles)
    , m_LeftBy(leftBy)
    , m_ClickedTile(clickedTile)
    , m_DurationMs(GetLastActionDiffMs())
    , m_Boot(CLandingPage::WasLaunchedFromBoot())
{
    if (m_LpId == 0 && MetricLandingPage::GetPreviousLpId() != 0)
    {
        m_LpId = MetricLandingPage::GetPreviousLpId();
    }
}

bool MetricLandingPageNav::Write(RsonWriter* rw) const
{
    rtry
    {
        rcheckall(MetricLandingPage::Write(rw));
        rcheckall(rw->WriteUns("tb", m_ActivePage));
        rcheckall(rw->WriteUns("ht", m_HoverTiles));
        rcheckall(rw->WriteUns("lb", m_LeftBy));
        rcheckall(rw->WriteUns("ct", m_ClickedTile));
        rcheckall(rw->WriteUns("dt", m_DurationMs));
        rcheckall(rw->WriteBool("bo", m_Boot));

        return true;
    }
    rcatchall
    {
    }

    return false;
}

void MetricLandingPageNav::TabEntered(u32 tab)
{
    sm_Tab = tab;
    TouchLastAction();
}

u32 MetricLandingPageNav::GetTab()
{
    return sm_Tab;
}

void MetricLandingPageNav::TouchLastAction()
{
    sm_LastActionMs = sysTimer::GetSystemMsTime();
}

u32 MetricLandingPageNav::GetLastActionMs()
{
    return sm_LastActionMs;
}

u32 MetricLandingPageNav::GetLastActionDiffMs()
{
    const u32 timeNow = sysTimer::GetSystemMsTime();
    return (timeNow >= sm_LastActionMs) ? timeNow -sm_LastActionMs : 0;
}

void MetricLandingPageNav::CardFocusGained(u32 cardBit)
{
    // This is used for LP_TILES and CAROUSEL_TILES
    sm_HoverTiles |= (u32)(1u << cardBit);
}

void MetricLandingPageNav::SetFocusedCards(u32 bits)
{
    sm_HoverTiles = bits;
}

void MetricLandingPageNav::ClearFocusedCards()
{
    sm_HoverTiles = 0;
}

/// TileInfo ---------------------------------------------------
TileInfo::TileInfo()
    : m_Id(0)
    , m_StickerId(0)
{
}

void TileInfo::Set(const ScFeedPost& post)
{
    const ScFeedParam* stickerPar = post.GetParam(ScFeedParam::STICKER_ID);
    m_Id = ScFeedPost::GetTrackingId(post);
    m_StickerId = stickerPar != nullptr ? stickerPar->ValueAsHash().GetHash() : 0;
}

/// MetricLandingPageOnline ---------------------------------------------------
unsigned MetricLandingPageOnline::sm_TriggerLpId = 0;
unsigned MetricLandingPageOnline::sm_DataSource = 0;
TileInfo MetricLandingPageOnline::sm_Tile[ONLINE_LP_TILES];
unsigned MetricLandingPageOnline::sm_Locations = 0;

MetricLandingPageOnline::MetricLandingPageOnline(const eLandingPageOnlineDataSource& source, u32 mainOnDiskTileId)
    : m_Locations(0)
    , m_Boot(CLandingPage::WasLaunchedFromBoot())
{
    CollectData(source, mainOnDiskTileId);
}

void MetricLandingPageOnline::CollectData(const eLandingPageOnlineDataSource& source, u32 mainOnDiskTileId)
{
    switch (source)
    {
    case eLandingPageOnlineDataSource::Connecting:
    case eLandingPageOnlineDataSource::NoMpAccess:
    case eLandingPageOnlineDataSource::NoMpCharacter:
    case eLandingPageOnlineDataSource::InIntro:
    case eLandingPageOnlineDataSource::FromDisk:
        {
            m_Tile[0].m_Id = mainOnDiskTileId;
            m_Locations = 1;
        } break;
    case eLandingPageOnlineDataSource::None:
        {
            m_Tile[0].m_Id = ATSTRINGHASH("None", 0x1D632BA1);
            m_Locations = 1;
        } break;
    case eLandingPageOnlineDataSource::FromCloud:
        {
            m_Locations = sm_Locations;

            for (int i = 0; i < ONLINE_LP_TILES; ++i)
            {
                m_Tile[i] = sm_Tile[i];
            }
        } break;
    }
}

bool MetricLandingPageOnline::Write(RsonWriter* rw) const
{
    rtry
    {
        rcheckall(MetricLandingPage::Write(rw));
        rcheckall(rw->WriteUns("ol", m_Locations));
        rcheckall(rw->WriteBool("bo", m_Boot));

        char buffer[32];

        for (unsigned i=0; i< ONLINE_LP_TILES; ++i)
        {
            if (m_Tile[i].m_Id != 0)
            {
                formatf(buffer, "o%u", i);
                rw->WriteUns(buffer, m_Tile[i].m_Id);
            }
        }

        for (unsigned i = 0; i < ONLINE_LP_TILES; ++i)
        {
            if (m_Tile[i].m_Id != 0) // The check is on the tile id so it's clearer when a sticker is not set
            {
                formatf(buffer, "s%u", i);
                rw->WriteUns(buffer, m_Tile[i].m_StickerId);
            }
        }

        return true;
    }
    rcatchall
    {
    }

    return false;
}

void MetricLandingPageOnline::Set(unsigned layoutPublishId, const uiCloudSupport::LayoutMapping* mapping, const CSocialClubFeedTilesRow* row,
                                  const ScFeedPost* (&persistentOnlineTiles)[4], const atHashString (&persistentOnDiskTiles)[4])
{
    sm_Locations = 0;

    for (int i = 0; i < ONLINE_LP_TILES; ++i)
    {
        sm_Tile[i] = TileInfo();
    }

    for (int i = 0; mapping != nullptr && i < mapping->m_numCards; ++i)
    {
        const uiCloudSupport::LayoutPos& pos = mapping->m_pos[i];
        const ScFeedTile* tile = row->GetTile(pos.m_x, pos.m_y, layoutPublishId);
        unsigned mappedPos = MetricLandingPage::MapPos(pos.m_x, pos.m_y);

        if (tile != nullptr && gnetVerify(mappedPos < ONLINE_LP_TILES))
        {
            sm_Tile[mappedPos].Set(tile->m_Feed);
            sm_Locations |= (unsigned)(1u << mappedPos);
        }
    }

    for (int i = 0; mapping != nullptr && i < 4; ++i)
    {
        unsigned mappedPos = MetricLandingPage::MapPos(2, i);

        if (gnetVerify(mappedPos < ONLINE_LP_TILES))
        {
            if (persistentOnlineTiles[i] != nullptr)
            {
                sm_Tile[mappedPos].Set(*persistentOnlineTiles[i]);
            }
            else
            {
                sm_Tile[mappedPos].m_Id = persistentOnDiskTiles[i];
            }

            sm_Locations |= (unsigned)(1u << mappedPos);
        }
    }
}

bool MetricLandingPageOnline::HasData()
{
    return sm_Locations != 0;
}

/// MetricLandingPagePersistent ---------------------------------------------------
unsigned MetricLandingPagePersistent::sm_TriggerLpId = 0;
atHashString MetricLandingPagePersistent::sm_Tiles[MAX_TILES];

MetricLandingPagePersistent::MetricLandingPagePersistent()
    : m_Boot(CLandingPage::WasLaunchedFromBoot())
{
    sysMemSet(m_Count, 0, sizeof(m_Count));

    CollectData();
}

void MetricLandingPagePersistent::CollectData()
{
    const unsigned rowIdx = 0;

    for (unsigned i = 0; i < MAX_TILES; ++i)
    {
        m_Count[i] = (sm_Tiles[i] != atHashString()) ? 0 : -1;

        const CSocialClubFeedTilesRow* row = sm_Tiles[i] != atHashString() ? CSocialClubFeedTilesMgr::Get().Row(sm_Tiles[i]) : nullptr;

        if (row == nullptr)
        {
            continue;
        }

        int numTiles = 0;

        for (unsigned j = 0; j < ScFeedTile::MAX_CAROUSEL_ITEMS && numTiles < (int)CAROUSEL_TILES; ++j)
        {
            const ScFeedTile* tile = row->GetTile(j, rowIdx);

            if (tile)
            {
                m_Pin[i][numTiles].Set(tile->m_Feed);
                ++numTiles;
            }
        }

        m_Count[i] = numTiles;
    }
}

bool MetricLandingPagePersistent::Write(RsonWriter* rw) const
{
    rtry
    {
        rcheckall(MetricLandingPage::Write(rw));
        rcheckall(rw->WriteBool("bo", m_Boot));

        char buffer[32];

        for (unsigned p = 0; p < MAX_TILES; ++p)
        {
            int count = m_Count[p];

            if (count < 0)
            {
                continue;
            }

            formatf(buffer, "p%u", p + 1);
            rcheckall(rw->BeginArray(buffer, nullptr));

            for (int i = 0; i < count; ++i)
            {
                rcheckall(rw->WriteUns(nullptr, m_Pin[p][i].m_Id));
            }

            rcheckall(rw->End());

            formatf(buffer, "s%u", p + 1);
            rcheckall(rw->BeginArray(buffer, nullptr));

            for (int i = 0; i < count; ++i)
            {
                rcheckall(rw->WriteUns(nullptr, m_Pin[p][i].m_StickerId));
            }

            rcheckall(rw->End());
        }

        return true;
    }
    rcatchall
    {
    }

    return false;
}

void MetricLandingPagePersistent::Set(const atHashString* tiles, const unsigned tilesCount)
{
    for (unsigned i = 0; i < MAX_TILES; ++i)
    {
        sm_Tiles[i] = (i < tilesCount) ? tiles[i] : atHashString();
    }
}

bool MetricLandingPagePersistent::HasData()
{
    CSocialClubFeedTilesMgr& mgr = CSocialClubFeedTilesMgr::Get();

    // MetaReady is also set for failures
    if (mgr.Series().GetMetaDownloadState() != TilesMetaDownloadState::MetaReady || mgr.Heist().GetMetaDownloadState() != TilesMetaDownloadState::MetaReady)
    {
        return false;
    }

    for (unsigned i = 0; i < MAX_TILES; ++i)
    {
        if (sm_Tiles[i] != atHashString())
        {
            return true;
        }
    }

    return false;
}

/// MetricSaInNav ---------------------------------------------------
unsigned MetricSaInNav::sm_NavId = 0;

MetricSaInNav::MetricSaInNav(const unsigned product)
    : m_NavId(GetNavId())
    , m_Path(GetPurchaseLocationHash(CNetworkTelemetry::GetPurchaseMetricInformation().GetFromLocation()).GetHash())
    , m_Product(product)
{
    m_LpId = GetCleanedLpId(m_LpId, CNetworkTelemetry::GetPurchaseMetricInformation().GetFromLocation());
}

bool MetricSaInNav::Write(RsonWriter* rw) const
{
    rtry
    {
        rcheckall(MetricLandingPage::Write(rw));
        rcheckall(rw->WriteUns("nd", m_NavId));
        rcheckall(rw->WriteUns("pa", m_Path));
        rcheckall(rw->WriteUns("pr", m_Product));

        return true;
    }
    rcatchall
    {
    }

    return false;
}

void MetricSaInNav::CreateNavId()
{
    sm_NavId = (unsigned)netRandom::GetIntCrypto();
}

void MetricSaInNav::ClearNavId()
{
    sm_NavId = 0;
}

unsigned MetricSaInNav::GetCleanedLpId(const unsigned lpid, const eStorePurchaseLocation location)
{
    // PIA wants these to count as outside of the landing page even though technically we're on the landing page
    if (location == SPL_PAUSE_PLAY_SP || location == SPL_CHARACTER_SELECTION_WHEEL)
    {
        return 0;
    }

    // If we have a previous lpid and script tells us we came from a landing page tile then use the lpid from back then.
    if (lpid == 0 && MetricLandingPage::GetPreviousLpId() != 0 && 
        (location == SPL_LANDING_MP || location == SPL_PRIORITY_FEED || location == SPL_WHATS_NEW))
    {
        gnetDebug2("Setting the previous lpid %u in the store metric", MetricLandingPage::GetPreviousLpId());
        return MetricLandingPage::GetPreviousLpId();
    }

    return lpid;
}

/// MetricSaOutNav ---------------------------------------------------
unsigned MetricSaOutNav::sm_Upgrade = 0;
unsigned MetricSaOutNav::sm_Membership = 0;

MetricSaOutNav::MetricSaOutNav()
    : m_NavId(MetricSaInNav::GetNavId())
    , m_Path(GetPurchaseLocationHash(CNetworkTelemetry::GetPurchaseMetricInformation().GetFromLocation()).GetHash())
    , m_Upgrade(sm_Upgrade)
    , m_Membership(sm_Membership)
{
    m_LpId = MetricSaInNav::GetCleanedLpId(m_LpId, CNetworkTelemetry::GetPurchaseMetricInformation().GetFromLocation());
}

bool MetricSaOutNav::Write(RsonWriter* rw) const
{
    rtry
    {
        rcheckall(MetricLandingPage::Write(rw));
        rcheckall(rw->WriteUns("nd", m_NavId));
        rcheckall(rw->WriteUns("pa", m_Path));
        rcheckall(rw->WriteUns("up", m_Upgrade));
        rcheckall(rw->WriteUns("me", m_Membership));

        return true;
    }
    rcatchall
    {
    }

    return false;
}

void MetricSaOutNav::SpUpgradeBought(const unsigned upgrade)
{
    sm_Upgrade = upgrade;
}

void MetricSaOutNav::GtaMembershipBought(const unsigned membership)
{
    sm_Membership = membership;
}

void MetricSaOutNav::ClearBought()
{
    sm_Upgrade = 0;
    sm_Membership = 0;
}

#endif // GEN9_LANDING_PAGE_ENABLED

/////////////////////////////////////////////////////////////////////////////// PLAYER_CHECK
MetricPLAYER_CHECK::MetricPLAYER_CHECK()
{
	m_pRank = StatsInterface::GetIntStat(STAT_RANK_FM.GetStatId());
}

bool MetricPLAYER_CHECK::Write(RsonWriter* rw) const
{
	bool result = MetricPlayStat::Write(rw);
	result &= rw->WriteInt("a", m_pRank);
	return result;
}

/////////////////////////////////////////////////////////////////////////////// ATTACH_PED_AGG
MetricATTACH_PED_AGG::MetricATTACH_PED_AGG(int pedCount, bool clone, s32 account)
	: m_pedCount(pedCount)
	, m_clone(clone)
	, m_sessionType(NetworkInterface::GetSessionTypeForTelemetry())
	, m_missionType(CNetworkTelemetry::GetMatchStartedId())
	, m_rootContentId(CNetworkTelemetry::GetRootContentId())
	, m_playerAccount(account)
{

}

bool MetricATTACH_PED_AGG::Write(RsonWriter* rw) const
{
	bool result = MetricPlayStat::Write(rw);
	result &= rw->WriteInt("a", m_pedCount);
	result &= rw->WriteBool("b", m_clone);
	result &= rw->WriteInt("c", m_sessionType);
	result &= rw->WriteInt("d", m_missionType);
	result &= rw->WriteInt("e", m_rootContentId);
	result &= rw->WriteUns("f", m_playerAccount);
	return result;
}

//eof 
