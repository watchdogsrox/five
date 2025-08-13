//
// NetworkMetrics.h
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef _NETWORKMETRICS_H_
#define _NETWORKMETRICS_H_

#include "vector/vector3.h"
#include "rline/rltelemetry.h"
#include "network/networkinterface.h"
#include "network/NetworkTypes.h"
#include "network/Live/NetworkMetricsChannels.h"
#include "Stats/StatsTypes.h"
#include "SaveLoad/savegame_defines.h"

#if RSG_OUTPUT
#include "atl/hashstring.h"
#endif //RSG_OUTPUT

#include "atl/string.h"
#include "system/EndUserBenchmark.h"
#include "rline/socialclub/rlsocialclubcommon.h"

namespace rage
{
	class RsonWriter;
}

namespace uiCloudSupport
{
	struct LayoutMapping;
}

class CSocialClubFeedTilesRow;
class ScFeedPost;
enum class eLandingPageOnlineDataSource;
class uiPageDeckMessageBase;

CompileTimeAssert(MAX_TELEMETRY_CHANNEL <= rlTelemetryPolicies::MAX_NUM_LOG_CHANNELS);
OUTPUT_ONLY( const char* GetTelemetryChannelName(const u32 channel); )

enum TelemetrysLogLevel 
{
	 LOGLEVEL_VERYHIGH_PRIORITY
	,LOGLEVEL_HIGH_PRIORITY
	,LOGLEVEL_MEDIUM_PRIORITY
	,LOGLEVEL_LOW_PRIORITY
	,LOGLEVEL_VERYLOW_PRIORITY

	//High Volume metric - ie metrics that are likely to disrupt normal metrics submissions.
	,LOGLEVEL_HIGH_VOLUME

	//Debug log level
	,LOGLEVEL_DEBUG1
	,LOGLEVEL_DEBUG2

	//Maximum log level.
	,LOGLEVEL_MAX = rlTelemetryPolicies::MAX_NUM_LOG_CHANNELS-1

	,LOGLEVEL_DEBUG_NEVER
};
OUTPUT_ONLY( const char* GetTelemetryLogLevelName(const u32 loglevel); )


//////////////////////////////////////////////////////////////////////////
// Metrics definitions
//////////////////////////////////////////////////////////////////////////

//PURPOSE
// Stores common fields for all SPEND metrics
class SpendMetricCommon
{
public:

	static void SetCommonFields(int properties, int properties2, int heists, bool WINDFALL_METRICS_ONLY(windfall))
	{
		sm_properties = properties;
		sm_properties2 = properties2;
		sm_heists = heists;
#if NET_ENABLE_WINDFALL_METRICS
		sm_windfall = windfall;
#endif
	}

	static void SetDiscount(int discount)
	{
		sm_discount = discount;
	}

	static int sm_properties;
	static int sm_properties2;
	static int sm_heists;
	static int sm_discount;
#if NET_ENABLE_WINDFALL_METRICS
	static bool sm_windfall;
#endif
};

//PURPOSE
//   Base class for all declared metrics.
class MetricPlayStat : public rlMetric
{
public:
	enum
	{
		MAX_STRING_LENGTH   = 32
	};

	virtual bool Write(RsonWriter* /*rw*/) const
	{
		return true;
	}
};


//PURPOSE
//   Base class for all metrics that need to send the player coords.
//
//PARAMS SENT:
//   "c":[m_X,m_Y,m_Z]
//
class MetricPlayerCoords : public MetricPlayStat
{
public:
	MetricPlayerCoords();
	virtual bool Write(RsonWriter* rw) const;
	void MarkPlayerCoords();

#if RSG_OUTPUT
	Vector3	GetCoords() {  return Vector3(m_X, m_Y, m_Z); }
#endif	// RSG_OUTPUT

private:

	float m_X;
	float m_Y;
	float m_Z;
};


//PURPOSE
//   Convenient way of passing loads of params into the metric PLAYSTATS_JOB_ACTIVITY_ENDED.
//
struct  sJobInfo
{
public:
	static const u32 NUM_INTS  = 35;
	static const u32 NUM_U32   = 23;
	static const u32 NUM_BOOLS = 9;
	static const u32 NUM_U64   = 4;

public:
	sJobInfo() 
		: m_intsused(0)
		, m_boolused(0)
		, m_u32used(0)
		, m_u64used(0) 
	{}

	const sJobInfo& operator=(const sJobInfo& other)
	{
		m_intsused = other.m_intsused;
		m_boolused = other.m_boolused;
		m_u32used  = other.m_u32used;
		m_u64used  = other.m_u64used;

		for(u32 i=0; i<sJobInfo::NUM_U64; i++)
			m_dataU64[i] = other.m_dataU64[i];

		for(u32 i=0; i<sJobInfo::NUM_U32; i++)
			m_dataU32[i] = other.m_dataU32[i];

		for(u32 i=0; i<sJobInfo::NUM_INTS; i++)
			m_dataINTS[i] = other.m_dataINTS[i];

		for(u32 i=0; i<sJobInfo::NUM_BOOLS; i++)
			m_dataBOOLEANS[i] = other.m_dataBOOLEANS[i];

		safecpy(m_MatchCreator,  other.m_MatchCreator,  COUNTOF(other.m_MatchCreator));
		safecpy(m_UniqueMatchId, other.m_UniqueMatchId, COUNTOF(other.m_UniqueMatchId));

		return *this;
	}

public:
	u32  m_intsused;
	u32  m_boolused;
	u32  m_u32used;
	u32  m_u64used;

	int  m_dataINTS[NUM_INTS];
	bool m_dataBOOLEANS[NUM_BOOLS];
	u32  m_dataU32[NUM_U32];
	u64  m_dataU64[NUM_U64];
	char m_MatchCreator[MetricPlayStat::MAX_STRING_LENGTH];
	char m_UniqueMatchId[MetricPlayStat::MAX_STRING_LENGTH];
};

//PURPOSE
//   Metric sent to indicate whether a player has a multiplayer subscription or not
//
class MetricMultiplayerSubscription : public MetricPlayStat
{
    RL_DECLARE_METRIC(MPSUB, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

public:
    MetricMultiplayerSubscription(const bool hasSubscription)
        : m_HasSubscription(hasSubscription)
    {

    }

    bool Write(RsonWriter* rw) const;

private:

    bool m_HasSubscription; 
};

//PURPOSE
//   Metric with timing details of transitions into and out of multiplayer
//
class MetricTransitionTrack : public MetricPlayStat
{
	RL_DECLARE_METRIC(TRN, TELEMETRY_CHANNEL_FLOW, LOGLEVEL_VERYHIGH_PRIORITY);

public:

	struct sTransitionStage
	{
	public:

		sTransitionStage() 
			: m_Slot(0)
			, m_Hash(0)
			, m_TimeInMs(0)
			, m_Param1(0)
			, m_Param2(0)
			, m_Param3(0)
		{}

		sTransitionStage(const u32 nSlot, u32 nHash, const int nParam1, const int nParam2, const int nParam3) 
			: m_Slot(nSlot)
			, m_Hash(nHash)
			, m_Param1(nParam1)
			, m_Param2(nParam2)
			, m_Param3(nParam3)
			, m_TimeInMs(0)
		{}

		bool Write(RsonWriter* rw) const;

		sTransitionStage& operator=(const sTransitionStage& other);

	public:
		u32 m_Slot; 
		u32	m_Hash;
		u32 m_TimeInMs;
		int m_Param1;
		int m_Param2;
		int m_Param3;
	};

public:

	MetricTransitionTrack()
		: m_Type(0)
		, m_Param1(0)
		, m_Param2(0)
		, m_Param3(0)
		, m_nStages(0)
		, m_LastStageWipe(0)
	{

	}

	void Start(const int nType, const int nParam1, const int nParam2, const int nParam3);
	void ClearStages();
	bool AddStage(u32 nSlot, u32 nHash, int nParam1, int nParam2, int nParam3, u32 nPreviousTime);
	void Finish();

	virtual bool Write(RsonWriter* rw) const;
	void LogContents();
	unsigned GetLength() { return m_DummyWriter.Length(); }

private:

	unsigned m_Identifier; 
	int m_Type;
	int m_Param1;
	int m_Param2;
	int m_Param3;
	
	static const unsigned MAX_TRANSITION_STAGES = 30; 
	sTransitionStage m_data[MAX_TRANSITION_STAGES];
	unsigned m_nStages; 

	unsigned m_LastStageWipe; 

	// dummy writer
	static const unsigned MAX_STAGE_WRITE_SIZE = 128; 
	char m_DummyBuffer[RL_TELEMETRY_DEFAULT_WORK_BUFFER_SIZE];
	RsonWriter m_DummyWriter;
};

//PURPOSE
//   Metric sent with the result of the query done to find about session's. Send the details
//    of the results received.
//
//PARAMS SENT:
//   {"d":[m_index,m_sessionid,m_index,m_sessionid]}
//
class MetricMatchmakingQueryResults : public MetricPlayStat
{
	RL_DECLARE_METRIC(MMF, TELEMETRY_CHANNEL_FLOW, LOGLEVEL_VERYHIGH_PRIORITY);

public:

	struct sCandidateData
	{
	public:

		sCandidateData() 
			: m_sessionid(0)
			, m_usedSlots(0)
			, m_maxSlots(0)
			, m_fromQueries(0)
			, m_TimeAttempted(0)
			, m_ResultCode(0)
		{}

		bool Write(RsonWriter* rw) const;

		sCandidateData& operator=(const sCandidateData& other);

	public:
		u64   m_sessionid;
		u32   m_usedSlots;
		u32   m_maxSlots;
		u32	  m_fromQueries;
		u32	  m_TimeAttempted;
		u32	  m_ResultCode;
	};

public:

	MetricMatchmakingQueryResults(const u64 nUniqueMatchmakingID, const unsigned nGameMode, const unsigned nCandidateIndex, const unsigned nTimeTaken, bool bHosted) 
		: m_UniqueID(nUniqueMatchmakingID)
		, m_nGameMode(nGameMode)
		, m_CandidateIndex(nCandidateIndex)
		, m_TimeTaken(nTimeTaken)
		, m_bHosted(bHosted)
		, m_nCandidates(0)
	{

	}

	bool AddCandidate(const sCandidateData& data);
	virtual bool Write(RsonWriter* rw) const;

private:

	u64 m_UniqueID;
	unsigned m_nGameMode;
	unsigned m_CandidateIndex;
	unsigned m_TimeTaken;
	bool m_bHosted;
	sCandidateData m_Candidates[NETWORK_MAX_MATCHMAKING_CANDIDATES];
	unsigned m_nCandidates; 
};


//PURPOSE
//   Metric sent with the query done to find about session's. Send the details
//    of the search done.
//
//PARAMS SENT:
//   {"gm":m_gamemode,"gt":m_gametype,"d":[m_id,m_value,m_id,m_value]}
//
class MetricMatchmakingQueryStart : public MetricPlayStat
{
	RL_DECLARE_METRIC(MMS, TELEMETRY_CHANNEL_FLOW, LOGLEVEL_VERYHIGH_PRIORITY);

private:
	static const u32 MAX_NUM_FILTERIDS = 20;

	struct sData
	{
		static const u32 INVALID_ID = 0;

	public:
		sData() : m_id(INVALID_ID), m_value(0) {;}

		bool Write(RsonWriter* rw) const;

	public:
		u32  m_id;
		u32  m_value;
	};

public:
	MetricMatchmakingQueryStart(const u32 gamemode, const u64 uid, const u32 queriesMade) 
		: m_gamemode(gamemode)
		, m_uid(uid)
		, m_queriesMade(queriesMade)
	{}

	bool AddFilterValue(const u32 id, const u32* value);

	virtual bool Write(RsonWriter* rw) const;

private:
	sData  m_data[MAX_NUM_FILTERIDS];
	u32    m_gamemode;
	u64    m_uid;
	u32    m_queriesMade;
};


//PURPOSE
//   Metric sent when we fail to join a session.
//
class MetricJoinSessionFailed : public MetricPlayStat
{
	RL_DECLARE_METRIC(JOINFAILED, TELEMETRY_CHANNEL_FLOW, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	MetricJoinSessionFailed(const u64 sid, const u64 uid, const int bailReason) : m_sessionid(sid), m_uid(uid), m_bailReason(bailReason) {}
	virtual bool Write(RsonWriter* rw) const;

private:
	u64   m_sessionid;
	u64   m_uid;
	int   m_bailReason;
};


//PURPOSE
//   Metric sent when a job is started.
//
//PARAMS SENT:
//   {
//      "act":m_activity,
//		"frtotal":m_numFriendsTotal,
//		"frtitle":m_numFriendsInSameTitle,
//		"frsession":m_numFriendsInMySession,
//		"jointime":m_oncalljointime,
//		"joinstate":m_oncalljoinstate
//   }
//
class MetricJOB_STARTED : public MetricPlayStat
{
public:
	struct sMatchStartInfo
	{
		u32 m_numFriendsTotal;
		u32 m_numFriendsInSameTitle;
		u32 m_numFriendsInMySession;
		u32 m_oncalljointime;
		u32 m_oncalljoinstate;
		u32 m_missionDifficulty;
		u32 m_MissionLaunch;
	};

public:
	RL_DECLARE_METRIC(JOB_START, TELEMETRY_CHANNEL_FLOW, LOGLEVEL_HIGH_PRIORITY);

public:
	explicit MetricJOB_STARTED(const char* activity, const sMatchStartInfo& info);

	virtual bool Write(RsonWriter* rw) const
	{
		bool result = MetricPlayStat::Write(rw);
		result = result && rw->WriteString("act", m_activity);
		result = result && rw->WriteUns("frtotal", m_numFriendsTotal);
		result = result && rw->WriteUns("frtitle", m_numFriendsInSameTitle);
		result = result && rw->WriteUns("frsession", m_numFriendsInMySession);
		result = result && rw->WriteUns("jointime", m_oncalljointime);
		result = result && rw->WriteUns("joinstate", m_oncalljoinstate);
		result = result && rw->WriteUns("md", m_missionDifficulty);
		result = result && rw->WriteUns("ml", m_MissionLaunch);
		result = result && rw->WriteBool("vc", m_usedVoiceChat);
		return result;
	}

private:
	char m_activity[MetricPlayStat::MAX_STRING_LENGTH];
	u32 m_numFriendsTotal;
	u32 m_numFriendsInSameTitle;
	u32 m_numFriendsInMySession;
	u32 m_oncalljointime;
	u32 m_oncalljoinstate;
	u32 m_missionDifficulty;
	u32 m_MissionLaunch;
	bool m_usedVoiceChat;    // was the voice chat used during the previous corona
};


//PURPOSE
//   Base class for multiplayer session info.
//
//PARAMS SENT
//   {
//      "id":m_SessionId,
//		"st":1
//   }
//
class MetricMpSessionId : public MetricPlayStat
{
public:

	explicit MetricMpSessionId(const u64 sessionId, const bool isTransitioning) 
		: m_SessionId(sessionId)
		, m_isTransitioning(isTransitioning)
	{;}

	virtual bool Write(RsonWriter* rw) const	{
		bool result = MetricPlayStat::Write(rw);

		result = result && rw->WriteInt64("id", static_cast<s64>(m_SessionId));

		if (m_isTransitioning)
			result = result && rw->WriteInt("st", 1);

		return result;
	}

private:
	u64 m_SessionId;
	bool m_isTransitioning;
};

//PURPOSE
//   Base class for multiplayer session info.
//
//PARAMS SENT
//   {
//      "mode":m_GameMode,
//		"pub":m_PubSlots,
//		"priv":m_PrivSlots
//   }
//
class MetricMpSession : public MetricMpSessionId
{
public:
	MetricMpSession(const bool isTransitioning);

	virtual bool Write(RsonWriter* rw) const
	{
		return this->MetricMpSessionId::Write(rw)
			&& rw->WriteUns("mode", m_GameMode)
			&& rw->WriteUns("pub", m_PubSlots)
			&& rw->WriteUns("priv", m_PrivSlots);
	}

private:
	u8 m_GameMode;
	u8 m_PubSlots;
	u8 m_PrivSlots;
};


//PURPOSE
//   Metric sent when a job is ended.
//
//PARAMS SENT
//   {
//      "mtkey":m_matchmakingKey,
//		"exe":m_exeSize,
//		"mc":m_jobInfo.m_MatchCreator,
//		"mid":m_jobInfo.m_UniqueMatchId,
//		"i":[]
//		"b":[]
//		"u":[]
//		"u64":[]
//		"limit":m_CrewLimit,
//		"plid":m_playlistidi
//   }
//
class MetricJOB_ENDED : public MetricMpSession
{
	RL_DECLARE_METRIC(JOB_END, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

public:
	MetricJOB_ENDED(const sJobInfo& jobInfo, const char* playlistid);
	virtual bool Write(RsonWriter* rw) const;

protected:
	sJobInfo m_jobInfo;
	u8 m_CrewLimit;
	u32 m_matchmakingKey;
	s64 m_exeSize;
	bool m_usedVoiceChat;
	char m_playlistid[MetricPlayStat::MAX_STRING_LENGTH];
	bool m_closedJob;
	bool m_fromClosedFreemode;
	bool m_privateJob;
	bool m_fromPrivateFreemode;
	bool m_aggregateScore;
};

//PURPOSE
//   Metric sent when a B job is ended.
//
class MetricJOBBENDED : public MetricJOB_ENDED
{
	RL_DECLARE_METRIC(BEND, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

public:
	MetricJOBBENDED(const sJobInfo& jobInfo, const char* playlistid) : MetricJOB_ENDED(jobInfo, playlistid){}
};

//PURPOSE
//   Metric sent when a LTS job is ended.
//
class MetricJOBLTS_ENDED : public MetricJOB_ENDED
{
	RL_DECLARE_METRIC(LTS_END, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

public:
	MetricJOBLTS_ENDED(const sJobInfo& jobInfo, const char* playlistid) : MetricJOB_ENDED(jobInfo, playlistid){}
};

//PURPOSE
//   Metric sent when a LTS job round is ended.
//
class MetricJOBLTS_ROUND_ENDED : public MetricJOB_ENDED
{
	RL_DECLARE_METRIC(LTS_ROUND_END, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

public:
	MetricJOBLTS_ROUND_ENDED(const sJobInfo& jobInfo, const char* playlistid) : MetricJOB_ENDED(jobInfo, playlistid){}
};

struct MetricSessionStart
{
	bool m_isTransition;
	char m_LocalIp[rage::netSocketAddress::MAX_STRING_BUF_SIZE];    // to add later
	rage::netNatType m_Nat;
	bool m_UsedRelay;
	char m_RelayIp[rage::netSocketAddress::MAX_STRING_BUF_SIZE];

	MetricSessionStart(bool isTransioning);

	bool Write(RsonWriter* rw) const;
};

class MetricInviteResult : public MetricPlayStat
{
	RL_DECLARE_METRIC(INVITE_RESULT, TELEMETRY_CHANNEL_FLOW, LOGLEVEL_VERYLOW_PRIORITY);

public:
	MetricInviteResult(
		const u64 sessionToken,
		const char* uniqueMatchId,
		const int gameMode,
		const unsigned inviteFlags,
		const unsigned inviteId,
		const unsigned inviteAction,
		const int inviteFrom,
		const int inviteTyoe,
		const rlGamerHandle& inviterHandle,
		const bool isFriend);
		
	virtual bool Write(RsonWriter* rw) const;

private:

	u64 m_SessionToken; 
	char m_UniqueMatchId[MetricPlayStat::MAX_STRING_LENGTH];
	int m_GameMode;
	unsigned m_InviteFlags;
	unsigned m_InviteId;
	unsigned m_InviteAction;
	int m_InviteType;	// enumerated in script
	int m_InviteFrom;	// enumerated in script
	rlGamerHandle m_Inviter; 
	bool m_IsFromFriend;
};

//PURPOSE
//   Metric sent when we enter a solo session
//
class MetricENTERED_SOLO_SESSION : public MetricPlayStat
{
	RL_DECLARE_METRIC(ENTERED_SOLO_SESSION, TELEMETRY_CHANNEL_FLOW, LOGLEVEL_VERYLOW_PRIORITY);

public:
	MetricENTERED_SOLO_SESSION(const u64 sessionId, const u64 sessionToken, const int gameMode, const eSoloSessionReason soloSessionReason, const eSessionVisibility sessionVisibility, const eSoloSessionSource soloSessionSource, const int userParam1, const int userParam2, const int userParam3);
	virtual bool Write(RsonWriter* rw) const;

private:
	
	u64 m_SessionId; 
	u64 m_SessionToken; 
	int m_GameMode;
	eSoloSessionReason m_SoloSessionReason;
	eSessionVisibility m_SessionVisibility;
	eSoloSessionSource m_SoloSessionSource;
	int m_UserParam1;
	int m_UserParam2;
	int m_UserParam3;
};

//PURPOSE
//   Metric sent when the game stalls beyond a certain threshold
//
class MetricSTALL_DETECTED : public MetricPlayStat
{
	RL_DECLARE_METRIC(STALL_DETECTED, TELEMETRY_CHANNEL_FLOW, LOGLEVEL_VERYLOW_PRIORITY);

public:
	MetricSTALL_DETECTED(const unsigned stallTimeMs, const unsigned networkTimeMs, const bool bIsTransitioning, const bool bIsLaunching, const bool bIsHost, const unsigned nTimeoutsPending, const unsigned sessionSize, const u64 sessionToken, const u64 sessionId);
	MetricSTALL_DETECTED(const unsigned stallTimeMs, const unsigned networkTimeMs);
	virtual bool Write(RsonWriter* rw) const;

private:

	unsigned m_StallTimeMs;
	unsigned m_NetworkTimeMs;
	bool m_bInMultiplayer;
	bool m_bInTransition;
	bool m_bIsLaunching;
	bool m_bIsHost;
	unsigned m_NumTimeoutsPending;
	unsigned m_SessionSize;
	u64 m_SessionToken; 
	u64 m_SessionId;
};

//PURPOSE
//   Metric sent when we bail from a network game
//
class MetricNETWORK_BAIL : public MetricPlayStat
{
	RL_DECLARE_METRIC(NETWORK_BAIL, TELEMETRY_CHANNEL_FLOW, LOGLEVEL_VERYLOW_PRIORITY);

public:
	MetricNETWORK_BAIL(const sBailParameters params,
					   const unsigned nSessionState,
					   const u64 sessionId,
					   const bool bIsHost,
					   const bool bJoiningViaMatchmaking,
					   const bool bJoiningViaInvite,
					   const unsigned nTime);

	virtual bool Write(RsonWriter* rw) const;

private:

	sBailParameters m_bailParams;
	unsigned m_nSessionState;
	u64 m_sessionId;
	bool m_bIsHost;
	bool m_bJoiningViaMatchmaking;
	bool m_bJoiningViaInvite;
	unsigned m_nTime;
};

//PURPOSE
//   Metric sent when we are kicked in a network game
//
class MetricNETWORK_KICKED : public MetricPlayStat
{
	RL_DECLARE_METRIC(MetricNETWORK_KICKED, TELEMETRY_CHANNEL_FLOW, LOGLEVEL_MEDIUM_PRIORITY);

public:
	MetricNETWORK_KICKED(const u64 nSessionToken,
						 const u64 nSessionId,
						 const u32 nGameMode,
						 const u32 nSource,
						 const u32 nPlayers,
						 const u32 nSessionState,
						 const u32 nTimeInSession,
						 const u32 nTimeSinceLastMigration,
						 const unsigned nNumComplaints,
						 const rlGamerHandle& hGamer);

	virtual bool Write(RsonWriter* rw) const;

private:

	u64 m_SessionToken; 
	u64 m_SessionId; 
	u32 m_nGameMode;
	u32 m_nSource;
	u32 m_nPlayers;
	u32 m_nSessionState;
	u32 m_nTimeInSession;
	u32 m_nTimeSinceLastMigration;
	unsigned m_NumComplaints;
	char m_szComplainerUserID[RL_MAX_USERID_BUF_LENGTH];
};

//PURPOSE
//   Metric sent when we complete a migration
//
class MetricSessionMigrated : public MetricPlayStat
{
public:

	RL_DECLARE_METRIC(SMg, TELEMETRY_CHANNEL_FLOW, LOGLEVEL_VERYLOW_PRIORITY);

	enum CandidateCode
	{
		CANDIDATE_CODE_UNKNOWN,
		CANDIDATE_CODE_LOCALLY_DISCONNECTED,
		CANDIDATE_CODE_NOT_MIGRATING,
		CANDIDATE_CODE_LAUNCHING_TRANSITION,
		CANDIDATE_CODE_ESTABLISHED,
		CANDIDATE_CODE_NOT_ESTABLISHED,
	};

	enum CandidateResult
	{
		CANDIDATE_RESULT_DISCONNECTED,
		CANDIDATE_RESULT_CLIENT,
		CANDIDATE_RESULT_HOST,
		CANDIDATE_RESULT_ADDED_CLIENT,
		CANDIDATE_RESULT_ADDED_HOST,
	};

	MetricSessionMigrated();

	virtual bool Write(RsonWriter* rw) const;

	void Start(const u64 nMigrationId, const u32 nPosixTime, const u32 nGameMode);
	void AddCandidate(const char* szGamerName, const CandidateCode nCode = CANDIDATE_CODE_UNKNOWN);
	void SetCandidateResult(const char* szGamerName, const CandidateResult nResult);
	void Finish(const u32 nTimeTaken);

private:

	u64 m_MigrationId;
	u32 m_PosixTime; 
	u32 m_GameMode;
	unsigned m_Time;

	// candidate data
	struct sCandidate
	{
		sCandidate() 
			: m_Code(CANDIDATE_CODE_UNKNOWN)
			, m_Result(CANDIDATE_RESULT_DISCONNECTED)
		{
			m_Name[0] = '\0';
		}

		bool Write(RsonWriter* rw) const;

		CandidateCode m_Code : 8;
		CandidateResult m_Result : 8;
		char m_Name[RL_MAX_NAME_BUF_SIZE];
	};
	sCandidate m_Candidates[RL_MAX_GAMERS_PER_SESSION];
	unsigned m_NumCandidates; 
};

//PURPOSE
//   Metric sent when a session is hosted.
//
//PARAMS SENT
//   {
//      "mtkey":m_matchmakingKey,
//		"exe":m_exeSize,
//   }
//
class MetricSESSION_HOSTED : public MetricMpSession
{
	RL_DECLARE_METRIC(SES_HOST, TELEMETRY_CHANNEL_FLOW, LOGLEVEL_VERYLOW_PRIORITY);

public:
	MetricSESSION_HOSTED(const bool isTransitioning, const u32 matchmakingKey, const u64 nUniqueMatchmakingID, const u64 sessionToken, const u64 nTimeCreated);
	virtual bool Write(RsonWriter* rw) const;

private:
	u32 m_matchmakingKey;
	s64 m_exeSize;
	u8  m_OnlineJoinType;
	u64 m_nUniqueMatchmakingID;
	u64 m_nSessionToken;
	u64 m_nTimeCreated;
	MetricSessionStart m_start;
};

//PURPOSE
//   Metric sent when we join a session.
//PARAMS SENT
//   {
//      "p":m_numPlayers,
//      "mtkey":m_matchmakingKey,
//		"exe":m_exeSize,
//		"sctv":m_sctv
//   }
class MetricSESSION_JOINED : public MetricMpSession
{
	RL_DECLARE_METRIC(SES_JOIN, TELEMETRY_CHANNEL_FLOW, LOGLEVEL_MEDIUM_PRIORITY);

public:
	MetricSESSION_JOINED(const bool isTransitioning, const unsigned matchmakingKey, const u64 nUniqueMatchmakingID,	const u64 sessionToken,	const u64 timeCreated);
	virtual bool Write(RsonWriter* rw) const;

private:
	u32 m_numPlayers;
	u32 m_matchmakingKey;
	s64 m_exeSize;
	bool m_sctv;
	u8  m_OnlineJoinType;
	u64 m_nUniqueMatchmakingID;
	u64 m_nSessionToken;
	u64 m_nTimeCreated;
	MetricSessionStart m_start;
};


//PURPOSE
//   Metric sent when we leave a session.
//PARAMS SENT
//   {
//      "p":m_numPlayers,
//   }
class MetricSESSION_LEFT : public MetricMpSessionId
{
	RL_DECLARE_METRIC(SES_LEFT, TELEMETRY_CHANNEL_FLOW, LOGLEVEL_MEDIUM_PRIORITY);

	MetricSESSION_LEFT(const u64 sessionId, const bool isTransitioning);
	virtual bool Write(RsonWriter* rw) const;

private:
	u32 m_numPlayers;
};


//PURPOSE
//   Metric sent when we become the session host.
class MetricSESSION_BECAME_HOST : public MetricMpSessionId
{
	RL_DECLARE_METRIC(SES_IS_H, TELEMETRY_CHANNEL_FLOW, LOGLEVEL_LOW_PRIORITY);
	MetricSESSION_BECAME_HOST(const u64 sessionId) : MetricMpSessionId(sessionId, false) {;}
};


//PURPOSE
//   Metric sent when we become the session host.
class MetricBaseMultiplayerMissionDone
{
public:
	MetricBaseMultiplayerMissionDone (const u32  missionId, const u32 xpEarned, const u32 cashEarned, const u32 cashSpent);

	bool Write(RsonWriter* rw) const;

private:
	u32  m_MissionId;
	u32  m_XpEarned;
	u32  m_CashEarned;
	u32  m_CashSpent;
	u32  m_playtime;
};

//PURPOSE
//   Metric sent when we become the session host.

class MetricAMBIENT_MISSION_CRATEDROP : public MetricBaseMultiplayerMissionDone, public MetricPlayerCoords
{
	RL_DECLARE_METRIC(CRATE_DROP, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_MEDIUM_PRIORITY);

public:

	static const u32 MAX_CRATE_ITEM_HASHS = 8;

	MetricAMBIENT_MISSION_CRATEDROP(const u32  missionId, const u32 xpEarned, const u32 cashEarned, const u32 weaponHash, const u32 otherItem, const u32 enemiesKilled, const u32 (&_SpecialItemHashs)[MAX_CRATE_ITEM_HASHS], bool _CollectedBodyArmour);

	virtual bool Write(RsonWriter* rw) const;

private:
	u32  m_WeaponHash;
	u32  m_OtherItem;
	u32  m_EnemiesKilled;
	u32  m_SpecialItemHashs[MAX_CRATE_ITEM_HASHS];
	bool m_CollectedBodyArmour;
};


class MetricAMBIENT_MISSION_CRATE_CREATED : public MetricPlayStat
{
	RL_DECLARE_METRIC(CRATE_CREATED, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_MEDIUM_PRIORITY);

public:

	MetricAMBIENT_MISSION_CRATE_CREATED(float X, float Y, float Z);

	virtual bool Write(RsonWriter* rw) const;

private:
	float m_X;
	float m_Y;
	float m_Z;
};

class MetricAMBIENT_MISSION_RACE_TO_POINT : public MetricBaseMultiplayerMissionDone, public rlMetric
{
	RL_DECLARE_METRIC(RTOP, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_MEDIUM_PRIORITY);
public:
	struct sRaceToPointInfo {
		u32 m_LocationHash;
		u32 m_MatchId;
		u32 m_NumPlayers;
		bool m_RaceWon;
		Vector3 m_RaceStartPos;
		Vector3 m_RaceEndPos;
	};
	MetricAMBIENT_MISSION_RACE_TO_POINT(const u32  missionId, const u32 xpEarned, const u32 cashEarned, const sRaceToPointInfo& rtopInfo);
	virtual bool Write(RsonWriter* rw) const;
private:
	u32 m_LocationHash;
	u32 m_MatchId;
	u32 m_NumPlayers;
	bool m_RaceWon;
	float  m_StartX;
	float  m_StartY;
	float  m_StartZ;
	float  m_EndX;
	float  m_EndY;
	float  m_EndZ;
};

//PURPOSE
//   Metric sent to create reports for profile stats usage.
class MetricReadStatsByGroups : public MetricPlayStat
{
	RL_DECLARE_METRIC(PF_READBYGROUP, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

public:
	MetricReadStatsByGroups(const int triggerType) : m_triggerType(triggerType) {}

	virtual bool Write(RsonWriter* rw) const {
		return MetricPlayStat::Write(rw) 
			&& rw->WriteInt("tt", m_triggerType);
	}

private: 
	int m_triggerType;
};
#define  APPEND_PF_READBYGROUP(x) MetricReadStatsByGroups m(x); APPEND_METRIC(m)

//PURPOSE
//   Metric sent to create reports for profile stats usage.
class MetricReadStatsByGamer2 : public MetricPlayStat
{
	RL_DECLARE_METRIC(PF_READBYGAMER, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

public:
	MetricReadStatsByGamer2(const int triggerType, const int numgamers) 
		: m_triggerType(triggerType)
		, m_numgamers(numgamers)
	{}

	virtual bool Write(RsonWriter* rw) const {
		return MetricPlayStat::Write(rw) 
			&& rw->WriteInt("tt", m_triggerType)
			&& rw->WriteInt("g", m_numgamers);
	}

private: 
	int m_numgamers;
	int m_triggerType;
};
#define  APPEND_PF_READBYGAMER(x, numgamers) MetricReadStatsByGamer2 m(x, numgamers); APPEND_METRIC(m)

//PURPOSE
//   Metric sent to monitor the usage of the Freemode Creator - B*2145877
class MetricFREEMODE_CREATOR_USAGE: public MetricPlayStat
{
	RL_DECLARE_METRIC(CREATOR_USAGE, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_MEDIUM_PRIORITY);

public:

	MetricFREEMODE_CREATOR_USAGE(u32 timeSpent, u32 numberOfTries, u32 numberOfEarlyQuits, bool isCreating, u32 missionId);

	virtual bool Write(RsonWriter* rw) const;

private:
	u32 m_timeSpent;
	u32 m_numberOfTries;
	u32 m_numberOfEarlyQuits;
	bool m_isCreating;
	u32  m_missionId;

};

enum eStorePurchaseLocation
{
    SPL_UNKNOWN,
    SPL_SCUI,
    SPL_INGAME, // Pause menu 'Store' tab
    SPL_STORE, // All insufficient fund alerts
    SPL_PHONE, // Contact requests
    SPL_AMBIENT, // Most ambient script systems
    SPL_ONLINE_TAB, // Pause menu 'Online' tab
    SPL_STORE_GUN, // Direct access from shop
    SPL_STORE_CLOTH, // Direct access from shop
    SPL_STORE_HAIR, // Direct access from shop
    SPL_STORE_CARMOD, // Direct access from shop
    SPL_STORE_TATTOO, // Direct access from shop
    SPL_STORE_PERSONAL_MOD, // Direct access from shop
    SPL_STARTER_PACK, // Pause menu 'Online' tab starter pack option
    SPL_LANDING_MP, // A landing page ol-menu button
    SPL_LANDING_SP_UPSELL, // The single player upsell page
    SPL_PRIORITY_FEED, // The priority feed
    SPL_WHATS_NEW, // the whats new feed
    SPL_MEMBERSHIP_INGAME, // The membership button in-game from the online tab
    SPL_PAUSE_PLAY_SP, // The play single player button from the MP pause menu when SP is not owned
    SPL_CHARACTER_SELECTION_WHEEL // The character selection whell when SP is not owned
};

atHashString GetPurchaseLocationHash(const eStorePurchaseLocation location);

//PURPOSE
//   Metric sent to keep track of the purchase flow
class MetricPURCHASE_FLOW: public MetricPlayStat
{
	RL_DECLARE_METRIC(PURCHASE_FLOW, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_MEDIUM_PRIORITY);

public:

	struct PurchaseMetricInfos
	{
		static const int MAX_PRODUCT_IDS = 10;

		PurchaseMetricInfos();
		
		int m_storeId;
		u32 m_lastItemViewed;
		u32 m_lastItemViewedPrice;
		s64 m_bank;
		s64 m_evc;
		s64 m_pvc; 
		float m_pxr; 
		s64 m_charactersCash[MAX_NUM_MP_CHARS];

		int m_slot;

		static const int MAX_PRODUCT_ID_LENGTH = 20;

		char m_productIds[MAX_PRODUCT_IDS][MAX_PRODUCT_ID_LENGTH];
		int m_currentProductIdCount;

		void SaveCurrentCash();
		void Reset();
		void AddProductId(const atString& productId);

		eStorePurchaseLocation GetFromLocation() const { return m_fromLocation; }

		// The location we open the store in
		void SetFromLocation(const eStorePurchaseLocation location);

		// The location we will open the store in. This will overwrite the first call of SetFromLocation
		// Slightly risky in cases of sing outs or other errors during the transition but it should be fine for metric tracking
		void ReserveFromLocation(const eStorePurchaseLocation location);

	private:
		eStorePurchaseLocation m_fromLocation;
		eStorePurchaseLocation m_reservedLocation;
	};

	MetricPURCHASE_FLOW();

	virtual bool Write(RsonWriter* rw) const;

private:

	PurchaseMetricInfos m_metricInformation;

};


//PURPOSE
//   Metric sent to help track down the XP loss
class MetricXP_LOSS: public MetricPlayStat
{
	RL_DECLARE_METRIC(XP_LOSS, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_MEDIUM_PRIORITY);

public:
	
	MetricXP_LOSS(int previousXp, int newXp);

	virtual bool Write(RsonWriter* rw) const;
	static void AppendCallstack(const char* fmt, ...);
private:

	static const int MAX_CALLSTACK_SIZE = 16;   // MAX_CALLSTACK in script/thread.h
	static const int MAX_CALLSTACKFRAME_SIZE = 64;

	int m_PreviousXp;
	int m_NewXp;

	char m_CallstackFrames[MAX_CALLSTACK_SIZE][MAX_CALLSTACKFRAME_SIZE];


	static atString* sm_callstack;

};

//PURPOSE
//   Metric sent to keep track of the cheaters who might exploit Heists by pulling the cable
class MetricHEIST_SAVE_CHEAT: public MetricPlayStat
{
	RL_DECLARE_METRIC(HEIST_SAVE_CHEAT, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_MEDIUM_PRIORITY);

public:
	MetricHEIST_SAVE_CHEAT(int hashValue, int otherValue);

	virtual bool Write(RsonWriter* rw) const;

private:

	int m_hashValue;
	int m_otherHashValue;

};

//PURPOSE
//   Metric sent to keep track of the cheaters who might exploit Heists by pulling the cable
class MetricServiceFailed: public MetricPlayStat
{
	RL_DECLARE_METRIC(FAIL_SERV, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_VERYHIGH_PRIORITY);

public:
	enum eFailureTypes
	{
		//LOAD_FAILED_REASON_MAX - is 9, so I set it to 15 to cushion future changes of cloud savegame.
		 PS_HTTP_FAIL = 15
		,PS_DIRTY_READ
	};

public:
	MetricServiceFailed(const s64 uid, const int reason, const int errorcode) 
		: m_uid(uid)
		, m_reason(reason)
		, m_errorcode(errorcode)
	{
	}

	virtual bool Write(RsonWriter* rw) const
	{
		bool result = this->MetricPlayStat::Write(rw);
		result = result && rw->WriteInt64("g", m_uid);
		result = result && rw->WriteInt("r", m_reason);
		result = result && rw->WriteInt("e", m_errorcode);
		return result;
	}

private:
	s64 m_uid;
	int m_reason;
	int m_errorcode;
};
#define  APPEND_SERVICE_FAILURE(uid, reason, errorcode) MetricServiceFailed m(uid, reason, errorcode); APPEND_METRIC(m)

//   Metric for the Rockstar video editor
class MetricDIRECTOR_MODE: public MetricPlayStat
{
	RL_DECLARE_METRIC(DIRECTOR_MODE, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_MEDIUM_PRIORITY);

public:

	struct VideoClipMetrics
	{
		int m_characterHash;
		int m_timeOfDay;
		int m_weather;
		bool m_wantedLevel;
		int m_pedDensity;
		int m_vehicleDensity;
		bool m_restrictedArea;
		bool m_invulnerability;
	};

	MetricDIRECTOR_MODE(const VideoClipMetrics& values);

	virtual bool Write(RsonWriter* rw) const;

private:

	int m_characterHash;
	int m_timeOfDay;
	int m_weather;
	bool m_wantedLevel;
	int m_pedDensity;
	int m_vehicleDensity;
	bool m_restrictedArea;
	bool m_invulnerability;
};

//PURPOSE
//   Metric for the Rockstar video editor, sent when the project is saved
class MetricVIDEO_EDITOR_SAVE: public MetricPlayStat
{
	RL_DECLARE_METRIC(VID_ED_SAVE, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_MEDIUM_PRIORITY);

public:

	MetricVIDEO_EDITOR_SAVE(int clipCount, int* audioHash, int audioHashCount, u64 projectLength, u64 timeSpent);

	virtual bool Write(RsonWriter* rw) const;

private:

	static const int MAX_AUDIO_HASH = 10;  // see eEDIT_LIMITS::MAX_MUSIC_OBJECTS

	int m_clipCount;
	int m_audioHash[MAX_AUDIO_HASH];
	int m_usedAudioHash;
	u64 m_projectLength;
	u64 m_timeSpent;
};

//PURPOSE
//   Metric for the Rockstar video editor, sent when the project is uploaded
class MetricVIDEO_EDITOR_UPLOAD: public MetricPlayStat
{
	RL_DECLARE_METRIC(VID_ED_UP, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_MEDIUM_PRIORITY);

public:

	MetricVIDEO_EDITOR_UPLOAD(u64 videoLength, int m_scScore);

	virtual bool Write(RsonWriter* rw) const;

private:

	u64 m_videoLength;
	int m_scScore;
};


#if RSG_PC
//PURPOSE
//   Metric for the Benchmark FPS on PC
class MetricBENCHMARK_FPS: public MetricPlayStat
{
	RL_DECLARE_METRIC(BENCH_FPS, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_DEBUG1);

public:

	MetricBENCHMARK_FPS(const atArray<EndUserBenchmarks::fpsResult>& values);
	virtual bool Write(RsonWriter* rw) const;

private:

	static const int MAX_PASS_COUNT = 5;

	float m_fpsValues[MAX_PASS_COUNT][3];
	float m_under16ms[MAX_PASS_COUNT];
	float m_under33ms[MAX_PASS_COUNT]; 
	int m_passCount;

};

//   Metric for the Benchmark Percentiles on PC
class MetricBENCHMARK_P: public MetricPlayStat
{
	RL_DECLARE_METRIC(BENCH_P, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_DEBUG1);

public:

	MetricBENCHMARK_P(const atArray<EndUserBenchmarks::fpsResult>& values);
	virtual bool Write(RsonWriter* rw) const;

private:

	static const int MAX_PASS_COUNT = 5;
	static const int NUM_BENCHMARK_PERCENTILES = 14;

	int m_percentiles[MAX_PASS_COUNT][NUM_BENCHMARK_PERCENTILES];   
	int m_passCount;

};
#endif // RSG_PC

//   Metric to help track down the cheaters who try to spawn money
class MetricCASH_CREATED: public MetricPlayStat
{
	RL_DECLARE_METRIC(CASH_CREATED, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_MEDIUM_PRIORITY);

public:

	enum CashSpawnReason
	{
		Cash_Spawn_Ped_Killed
	};

	MetricCASH_CREATED(u64 uid, const rlGamerHandle& player, int amount, int hash);
	virtual bool Write(RsonWriter* rw) const;

private:

	u64 m_uid;
	char m_GamerHandle[RL_MAX_GAMER_HANDLE_CHARS];
	int m_amount;
	int m_hash;
};

//   Metric to keep track of the YouTube submission failure 
class MetricFAILED_YOUTUBE_SUB: public MetricPlayStat
{
	RL_DECLARE_METRIC(FAIL_YT_SUB, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_MEDIUM_PRIORITY);

public:

	MetricFAILED_YOUTUBE_SUB(int errorCode, const char* errorString, const char* uiFlowState);
	virtual bool Write(RsonWriter* rw) const;

private:

	static const int MAX_ERROR_STRING_SIZE = 32;
	static const int MAX_UI_FLOW_STATE_SIZE = 16;

	int m_errorCode;
	char m_errorString[MAX_ERROR_STRING_SIZE];
	char m_uiFlowState[MAX_UI_FLOW_STATE_SIZE];
};

class MetricSTARTERPACK_APPLIED: public MetricPlayStat
{
	RL_DECLARE_METRIC(SP_APPLIED, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_MEDIUM_PRIORITY);

public:

	MetricSTARTERPACK_APPLIED(int item, int amount);
	virtual bool Write(RsonWriter* rw) const;

private:

	int m_item;
	int m_amount;
};

// url:bugstar:3577057
#define MetricLAST_VEH MetricSecChange
class MetricLAST_VEH: public MetricPlayStat
{
	RL_DECLARE_METRIC(LAST_VEH, TELEMETRY_CHANNEL_MISC, LOGLEVEL_HIGH_PRIORITY);

public:

	static const int NUM_FIELDS = 7;

	MetricLAST_VEH();
	virtual bool Write(RsonWriter* rw) const;

public:
	int m_fields[NUM_FIELDS];
	u64 m_time;
	u64 m_session;
	char m_FromGH[RL_MAX_GAMER_HANDLE_CHARS];
	char m_ToGH[RL_MAX_GAMER_HANDLE_CHARS];
};

#define MetricFIRST_VEH MetricScriptEventSpam
class MetricFIRST_VEH: public MetricPlayStat
{
	RL_DECLARE_METRIC(FIRST_VEH, TELEMETRY_CHANNEL_MISC, LOGLEVEL_HIGH_PRIORITY);

public:
	
	MetricFIRST_VEH(int eventType, int ignoredEventCount);
	virtual bool Write(RsonWriter* rw) const;

	int m_eventType;
	int m_ignoredEventCount;
	char m_gamerHandle[RL_MAX_GAMER_HANDLE_CHARS];
	char m_playersInSession[MAX_NUM_PHYSICAL_PLAYERS][RL_MAX_GAMER_HANDLE_CHARS];
	int m_inSessionGamerhandles;
};


/////////////////////////////////////////////////////////////////////////////// OBJECT_REASSIGN_FAIL
class MetricOBJECT_REASSIGN_FAIL : public MetricPlayStat
{
	RL_DECLARE_METRIC(OBJECT_REASSIGN_FAIL, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

public:

	MetricOBJECT_REASSIGN_FAIL(const int playerCount, const int restartCount, const int ambObjCount, const int scrObjCount);

	virtual bool Write(RsonWriter* rw) const;

private:

	int m_playerCount;             // Number of players in the session
	int m_restartCount;            // Number of reassignment restarts (the process restarts if another player leaves while the process is in progress)
	int m_ambObjCount;             // Number of ambient objects on the reassignment list
	int m_scrObjCount;             // Number of script objects of the reassignment list
};

class MetricOBJECT_REASSIGN_INFO : public MetricPlayStat
{
	RL_DECLARE_METRIC(OBJECT_REASSIGN_INFO, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

public:

	MetricOBJECT_REASSIGN_INFO(const int playersInvolved, const int reassignCount, const float restartAvg, const float timeAvg, const int maxDuration);

	virtual bool Write(RsonWriter* rw) const;

private:

	int m_playersInvolved;             // Number of players in the session
	int m_reassignCount;           // Number of reassignments that took place in monitoring interval
	float m_restartAvg;            // Average number of restarts
	float m_timeAvg;               // Average time taken for the process to complete
	int m_maxDuration;             // Max time taken for the process to complete
};

class MetricUGC_NAV : public MetricPlayStat
{
	RL_DECLARE_METRIC(UGC_NAV, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

public:

	MetricUGC_NAV(const int feature, const int location);

	virtual bool Write(RsonWriter* rw) const;

private:

	int m_feature;
	int m_location;
};

class MetricBandwidthTest : public rlMetric
{
	RL_DECLARE_METRIC(BANDWIDTH_TEST, TELEMETRY_CHANNEL_MISC, LOGLEVEL_HIGH_PRIORITY);

public:
	MetricBandwidthTest(const u64 uploadbps, const u64 downloadbps, const unsigned source, const int resultCode);
	virtual bool Write(RsonWriter* rw) const;

private:
	u64 m_Uploadbps;
	u64 m_Downloadbps;
	unsigned m_Source;
	int m_ResultCode;
	int m_Region;
	char m_CountryCode[RLSC_MAX_COUNTRY_CODE_CHARS];
};

/// MetricPlatformInfos ---------------------------------------------

#if IS_GEN9_PLATFORM
class MetricPlatformInfos : public rlMetric
{
	RL_DECLARE_METRIC(PLATFORM, TELEMETRY_CHANNEL_MISC, LOGLEVEL_HIGH_PRIORITY);

public:
	MetricPlatformInfos();
	virtual bool Write(RsonWriter* rw) const;

private:
	sga::Hardware m_hardware;
};
#endif //IS_GEN9_PLATFORM

class MetricSP_SAVE_MIGRATION : public MetricPlayStat
{
	RL_DECLARE_METRIC(SPSM, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_MEDIUM_PRIORITY);

public:

	MetricSP_SAVE_MIGRATION(bool success, u32 savePosixTime, float completionPercentage, u32 lastCompletedMissionId, unsigned sizeOfSavegameDataBuffer);
	virtual bool Write(RsonWriter* rw) const;

private:

	bool m_success;
	u32 m_savePosixTime;
	float m_completionPercentage;
	u32 m_lastCompletedMissionId;
	unsigned m_sizeOfSavegameDataBuffer;
};

class MetricEventTriggerOveruse : public rlMetric
{
	RL_DECLARE_METRIC(NET_EV_AGG, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_MEDIUM_PRIORITY);
public:
	static const int MAX_EVENTS_REPORTED = 10;
	struct EventData
	{
		EventData() : m_eventType(-1), m_triggerCount(0), m_eventHash(0) {}
		EventData(int type, u32 count) : m_eventType(type), m_triggerCount(count), m_eventHash(0) {}
		int m_eventType;
		u32 m_eventHash;
		u32 m_triggerCount;
	};

	MetricEventTriggerOveruse(Vector3 pos, u32 rank, u32 triggerCount, u32 triggerDur, u32 totalCount, atFixedArray<EventData, MAX_EVENTS_REPORTED>& triggeredData, int sType, int mType, int contentId);
	virtual bool Write(RsonWriter* rw) const;

	
private:
	Vector3 m_playerPos;	// player coordinates
	u32 m_playerRank;		// the player's current rank

	u32 m_minTriggerCount;	// what the minimum number of network events is currently set to by tunable
	u32 m_triggerDuration;	// what the interval is currently set to by tunable
	u32 m_totalEventCount;	// total number of network events triggered within the set time
	
	
	// number of network events that triggered within the set time, limited at 10 elements
	atFixedArray<EventData, MAX_EVENTS_REPORTED> m_triggeredEventsData;
	
	
	int m_sessionType;		// type of session: freemode public, instanced, private etc.
	int m_missionType;		// what type of mission the player is currently in; null if not applicable
	int m_rootContentId;	// root content ID; null if not applicable

};

class MetricComms : public MetricPlayStat
{
	RL_DECLARE_METRIC(COMMS, TELEMETRY_CHANNEL_MISC, LOGLEVEL_MEDIUM_PRIORITY);

	enum eChatTelemetryCumulativeChatCategory
	{
		GAME_CHAT_CUMULATIVE=0,
		GAME_CHAT_TEAM_CUMULATIVE,
		NUM_CUMULATIVE_CHAT_TYPES
	};

	enum eChatTelemetryChatCategory
	{
		GAME_CHAT = 0,
		PHONE_TEXT,
		VOICE_CHAT,
		EMAIL_TEXT
	};

	enum eChatTelemetryChatType
	{
		CHAT_TYPE_NONE		= 0,
		CHAT_TYPE_PRIVATE	= BIT0,
		CHAT_TYPE_PUBLIC	= BIT1,
		CHAT_TYPE_TEAM		= BIT2,
		CHAT_TYPE_PARTY		= BIT3,
		CHAT_TYPE_PLATFORM	= BIT4,
		CHAT_TYPE_CREW		= BIT5,
		CHAT_TYPE_FRIENDS	= BIT6,
		CHAT_TYPE_ORG		= BIT7,
	};
		
public:
	struct CumulativeTextCommsInfo
	{
		CumulativeTextCommsInfo()
		{
			Reset();
		}

		void Reset()
		{
			m_CurrentChatId = 0;
			m_FirstChatTime = 0;
			m_lastChatTime = 0;
			m_messageCount = 0;
			m_totalMessageSize = 0;
		}

		void RecordMessage( const char* text );
		void SendTelemetry(eChatTelemetryCumulativeChatCategory chatType, class CNetGamePlayer* pReceiverPlayer = nullptr, const int durationModifier = 0);

		u32 m_CurrentChatId;
		u32 m_FirstChatTime;
		u32 m_lastChatTime;
		int m_messageCount;
		int m_totalMessageSize;
	};

	struct CumulativeVoiceCommsInfo
	{
		CumulativeVoiceCommsInfo()
		{
			Reset();
		}

		void Reset()
		{
			m_CurrentVoiceChatId = 0;
			m_LastTimeOfChatIdKeepAlive = 0;
			m_talking = false;
			m_voiceChatType = CHAT_TYPE_NONE;
			m_lastChatTime = 0;
			m_lastTimeStartedTalking = 0;
			m_totalTimeTalking = 0;
		}

		void UsingVoice(bool talking);
		void SendTelemetry(class CNetGamePlayer* pReceiverPlayer = nullptr);

		u32 m_CurrentVoiceChatId;
		u32 m_LastTimeOfChatIdKeepAlive;
		bool m_talking;
		int m_voiceChatType; // public, private, crew, etc, if this changes we send out the current values (MetricComms)
		u32 m_lastChatTime;
		u32 m_lastTimeStartedTalking;
		u32 m_totalTimeTalking;
	};

	MetricComms( eChatTelemetryChatCategory chatCateg, int chatType, u32 chatID, u64 sessionID, int sessionType, int messageCount, int totalCharCount, u32 duration, bool vChatDisable, int recAccID, const char* publicContentId, bool isInLobby, const rlGamerHandle* recGamerHandle = nullptr);
	MetricComms( const MetricComms::CumulativeTextCommsInfo& cumulativeInfo, eChatTelemetryChatCategory chatCateg, int chatType, u32 chatID, u64 sessionID, int sessionType, bool vChatDisable, int recAccID, const char* publicContentId, bool isInLobby, const int durationModifier = 0);
	MetricComms(const MetricComms::CumulativeVoiceCommsInfo& cumulativeInfo, u32 chatID, u64 sessionID, int sessionType, bool vChatDisable, int recAccID, const char* publicContentId, bool isInLobby);
	virtual bool Write(RsonWriter* rw) const;
		
private:
	eChatTelemetryChatCategory m_chatCateg;     // category of communication: game chat, phone text, voice chat
	int m_chatType;								// bitset, message type: private, public, crew, party (platform) etc.
	int m_chatID;								// ID to track this particular "session" of the chat
	u64 m_sessionID;							// session ID
	int m_sessionType;							// type of session: freemode public, instanced, private etc.
	int m_messageCount;							// count of messages written in game chat
	int m_charCount;							// count of characters in the written communication
	u32 m_duration;								// time spent in this type of communication
	bool m_vChatDisable;						// whether this player has voice chat turned off
	int m_recAccID;								// account ID of the receiver, for private chats with two players and phone texts
	char m_publicContentId[MetricPlayStat::MAX_STRING_LENGTH]; // public content ID if the player is in a lobby or instanced mode
	bool m_isInLobby;							// whether the player is in a lobby or not
	rlUserIdBuf m_rlUserId;						// fallback for email chat when recAccId isn't available
};

#if NET_ENABLE_MEMBERSHIP_METRICS
class MetricScMembership : public MetricPlayStat
{
	RL_DECLARE_METRIC(SCMEM, TELEMETRY_CHANNEL_MISC, LOGLEVEL_HIGH_PRIORITY);

public:

	MetricScMembership();
	virtual bool Write(RsonWriter* rw) const;

private:

	int m_MembershipTelemetryState;
	bool m_HasMembership; 
	u64 m_StartPosix; 
	u64 m_ExpiryPosix; 
};
#endif

typedef atMap<atString, int> PlayedWithDict;
typedef atMap<atString, int>::ConstIterator PlayedWithDictIter;
class MetricPlayedWith : public MetricPlayStat
{
	RL_DECLARE_METRIC(PLAYED_WITH, TELEMETRY_CHANNEL_MISC, LOGLEVEL_MEDIUM_PRIORITY);

public:
	static const u32 MAX_PLAYED_WITH_COUNT = 32;
	MetricPlayedWith(const PlayedWithDict&, u64);
	virtual bool Write(RsonWriter* rw) const;
private:
	char m_GamerHandles[MAX_PLAYED_WITH_COUNT][RL_MAX_GAMER_HANDLE_CHARS];
	u64 m_mpSessionId;
	int m_GamerHandlesCount;
};

class MetricLandingPage : public rlMetric
{
public:
    MetricLandingPage();

    virtual bool Write(RsonWriter* rw) const;

    static void CreateLpId();
    static void ClearLpId();
    static u32 GetLpId();

    static void StorePreviousLpId();
    static void ClearPreviousLpId();
    static u32 GetPreviousLpId();

    static unsigned MapPos(unsigned x, unsigned y);

public:
    static const unsigned ONLINE_LP_TILES = 12;
    static const unsigned LP_TILES = 12;

protected:
    static u32 sm_LpId;
    static u32 sm_PreviousLpId;

    u32 m_LpId;
};

#if GEN9_LANDING_PAGE_ENABLED
class MetricBigFeedNav : public rlMetric
{
    RL_DECLARE_METRIC(FEED_NAV, TELEMETRY_CHANNEL_MISC, LOGLEVEL_HIGH_PRIORITY);

public:

    MetricBigFeedNav(u32 hoverTile, u32 leftBy, u32 clickedTile, u32 durationMs, int skipTimerMs, bool whatsNew);
    virtual bool Write(RsonWriter* rw) const;

private:
    u32 m_LpId;
    u32 m_HoverTile;
    u32 m_LeftBy;
    u32 m_ClickedTile;
    u32 m_DurationMs;
    u32 m_SkipTimerMs;
    bool m_WhatsNew;
};

enum class PriorityFeedEnterState
{
    Unknown,
    Entered,
    NotEntered,
    InfoCollected
};

class MetricLandingPageMisc : public MetricLandingPage
{
    RL_DECLARE_METRIC(LP_MISC, TELEMETRY_CHANNEL_MISC, LOGLEVEL_HIGH_PRIORITY);

public:

    MetricLandingPageMisc();
    void CollectData();
    virtual bool Write(RsonWriter* rw) const;

    static bool HasData();
    static void SetPriorityBigFeedEnterState(const PriorityFeedEnterState state);
    static PriorityFeedEnterState GetPriorityBigFeedEnterState() { return sm_PriorityBigFeedEnterState; }

public:
    static unsigned sm_TriggerLpId;

private:
    static const unsigned MAX_CAROUSEL_POSTS = 30; // Fairly arbitrary numbers. There's no hard limit.
    static const unsigned MAX_PRIORITY_POSTS = 12;
    static PriorityFeedEnterState sm_PriorityBigFeedEnterState;

    u32 m_BigFeed[MAX_CAROUSEL_POSTS]; // The what's new page
    u32 m_PriorityBigFeed[MAX_PRIORITY_POSTS]; // The priority news page just before entering the IIS
    u32 m_BigFeedCount;
    u32 m_PriorityBigFeedCount;
    bool m_Boot;
};

class MetricLandingPageNav : public MetricLandingPage
{
    RL_DECLARE_METRIC(LP_NAV, TELEMETRY_CHANNEL_MISC, LOGLEVEL_HIGH_PRIORITY, METRIC_DEFAULT_OFF, METRIC_SAMPLE_ONCE);

public:
    MetricLandingPageNav(u32 clickedTile, u32 leftBy, u32 activePage);
    virtual bool Write(RsonWriter* rw) const;

    static void TabEntered(u32 tab);
    static u32 GetTab();

    static void TouchLastAction();
    static u32 GetLastActionMs();
    static u32 GetLastActionDiffMs();

    static void CardFocusGained(u32 cardBit);
    static void SetFocusedCards(u32 bits);
    static void ClearFocusedCards();

private:
    static u32 sm_Tab;
    static u32 sm_LastActionMs;
    static u32 sm_HoverTiles;

    u32 m_ActivePage; // The main page/view the user is currently on.
    u32 m_HoverTiles; // Bit mask of the tiles on the page
    u32 m_LeftBy; // The means by which the player left. Back, click, changing tab ...
    u32 m_ClickedTile; // The id of the clicked button or tile
    u32 m_DurationMs;
    bool m_Boot;
};

class TileInfo
{
public:
    TileInfo();
    void Set(const ScFeedPost& post);

public:
    unsigned m_Id;
    unsigned m_StickerId;
};

class MetricLandingPageOnline : public MetricLandingPage
{
    RL_DECLARE_METRIC(LP_ONLINE, TELEMETRY_CHANNEL_MISC, LOGLEVEL_HIGH_PRIORITY);

public:
    MetricLandingPageOnline(const eLandingPageOnlineDataSource& source, u32 mainOnDiskTileId);
    void CollectData(const eLandingPageOnlineDataSource& source, u32 mainOnDiskTileId);
    virtual bool Write(RsonWriter* rw) const;

    static void Set(unsigned layoutPublishId, const uiCloudSupport::LayoutMapping* mapping, const CSocialClubFeedTilesRow* row,
                    const ScFeedPost* (&persistentOnlineTiles)[4], const atHashString(&persistentOnDiskTiles)[4]);
    static bool HasData();

public:
    static unsigned sm_TriggerLpId;
    static unsigned sm_DataSource;

private:
    static TileInfo sm_Tile[ONLINE_LP_TILES];
    static unsigned sm_Locations;

    TileInfo m_Tile[ONLINE_LP_TILES];
    unsigned m_Locations;
    bool m_Boot;
};

class MetricLandingPagePersistent : public MetricLandingPage
{
    RL_DECLARE_METRIC(LP_PERSIST, TELEMETRY_CHANNEL_MISC, LOGLEVEL_HIGH_PRIORITY);

public:
    MetricLandingPagePersistent();
    void CollectData();
    virtual bool Write(RsonWriter* rw) const;

    static void Set(const atHashString* tiles, const unsigned tilesCount);
    static bool HasData();

public:
    static unsigned sm_TriggerLpId;

private:
    static const unsigned CAROUSEL_TILES = 16;
    static const unsigned MAX_TILES = 4;

    static atHashString sm_Tiles[MAX_TILES];

    TileInfo m_Pin[MAX_TILES][CAROUSEL_TILES];
    int m_Count[MAX_TILES]; // -1 .. nothing, 0 .. empty list, N ..number of entries
    bool m_Boot;
};

class MetricSaInNav : public MetricLandingPage
{
    RL_DECLARE_METRIC(SA_IN_NAV, TELEMETRY_CHANNEL_MISC, LOGLEVEL_HIGH_PRIORITY);

public:
    MetricSaInNav(const unsigned product);
    virtual bool Write(RsonWriter* rw) const;

    static void CreateNavId();
    static void ClearNavId();
    static unsigned GetNavId() { return sm_NavId; }
    static unsigned GetCleanedLpId(const unsigned lpid, const eStorePurchaseLocation location);

private:
    static unsigned sm_NavId;

    unsigned m_NavId;                   // unique ID to link with metric SA_OUT_NAV, to pair the two events; unique within the day is sufficient 
    unsigned m_Path;                    // path the player took (landing page>online>store, landing page>story>store, landing page>create>store, MP pause menu>store, character wheel>SP, creator pause menu>store etc.)
    unsigned m_Product;
};

class MetricSaOutNav : public MetricLandingPage
{
    RL_DECLARE_METRIC(SA_OUT_NAV, TELEMETRY_CHANNEL_MISC, LOGLEVEL_HIGH_PRIORITY);

public:
    MetricSaOutNav();
    virtual bool Write(RsonWriter* rw) const;

    static void SpUpgradeBought(const unsigned upgrade);
    static void GtaMembershipBought(const unsigned membership);
    static void ClearBought();

private:
    static unsigned sm_Upgrade;
    static unsigned sm_Membership;

    unsigned m_NavId;                   // unique ID to link with metric SA_IN_NAV, to pair the two events; unique within the day is sufficient 
    unsigned m_Path;                    // path the player took (landing page>online>store, landing page>story>store, landing page>create>store, MP pause menu>store, character wheel>SP, creator pause menu>store etc.)
    unsigned m_Upgrade;                 // whether the player bought singleplayer from the store
    unsigned m_Membership;              // whether the player bought membership from the store
};

#endif // GEN9_LANDING_PAGE_ENABLED

/////////////////////////////////////////////////////////////////////////////// PLAYER_CHECK
class MetricPLAYER_CHECK : public MetricPlayStat
{
	RL_DECLARE_METRIC(PLAYER_CHECK, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

public:

	MetricPLAYER_CHECK();

	virtual bool Write(RsonWriter* rw) const;

private:

	int m_pRank;                   // the player's current rank
};

/////////////////////////////////////////////////////////////////////////////// PLAYER_CHECK
class MetricATTACH_PED_AGG : public MetricPlayerCoords
{
	RL_DECLARE_METRIC(ATTACH_PED_AGG, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

public:

	MetricATTACH_PED_AGG(int pedCount, bool clone, s32 account);

	virtual bool Write(RsonWriter* rw) const;

private:
	
	int m_pedCount;
	bool m_clone;
	int m_sessionType;
	int m_missionType;
	int m_rootContentId;
	s32 m_playerAccount;
};

#endif // _NETWORKMETRICS_H_

//eof 

