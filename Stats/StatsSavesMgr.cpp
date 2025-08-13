//
// StatsSavesMgr.h
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//

//Rage Headers
#include <time.h>
#include "system/param.h"
#include "rline/rl.h"
#include "rline/rltelemetry.h"
#include "rline/ros/rlroscommon.h"
#include "rline/ros/rlros.h"
#include "string/stringutil.h"

#if __BANK
#include "bank/bank.h"
#endif

//Stats headers
#include "Stats/StatsSavesMgr.h"
#include "Stats/stats_channel.h"
#include "Stats/StatsInterface.h"

//Game headers
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_list.h"
#include "SaveLoad/savegame_defines.h"
#include "Network/NetworkInterface.h"
#include "Network/Network.h"
#include "Network/Sessions/NetworkSession.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "script/script_channel.h"
#include "script/script.h"
#include "Network/Cloud/Tunables.h"
#include "Network/Live/NetworkTelemetry.h"

#if !__NO_OUTPUT
#include "stats/StatsMgr.h"
#include "stats/StatsDataMgr.h"
XPARAM(spewDirtySavegameStats);
#endif // !__NO_OUTPUT


FRONTEND_STATS_OPTIMISATIONS()
SAVEGAME_OPTIMISATIONS()

XPARAM(disableOnlineStatsAreSynched);
PARAM(nompsavegame, "[stat_savemgr] Disable all savegames for multiplayer.");
XPARAM(lan);
XPARAM(nonetwork);
XPARAM(cloudForceAvailable);
PARAM(failmpload, "[stat_savemgr] Make any savegame fail to load.");
PARAM(failmploadalways, "[stat_savemgr] Make any savegame fail to load.");
PARAM(nompsavesometimes, "[stat_savemgr] Simulate save fail.");
PARAM(nomploadsometimes, "[stat_savemgr] Simulate load fail.");
PARAM(enableLocalMPSaveCache, "[stat_savemgr] Enable local cache of multiplayer savegames.");
PARAM(mpactivechar, "[stat_savemgr] Override active character.");
PARAM(mpactiveslots, "[stat_savemgr] Override numver of active slots.");
PARAM(changeLocalProfileTime, "[stat_savemgr] Override local profile stats flush time.");
PARAM(dirtyreadtimeoutPeriod, "[stat_savemgr] Override local profile stats flush timeout time.");
PARAM(changeLocalCloudTime, "[stat_savemgr] Override local cloud save time.");
PARAM(dirtycloudreadtimeoutPeriod, "[stat_savemgr] Override local cloud save timeout time.");

#if !__FINAL
PARAM(assertOnCanSaveFail, "[stat_savemgr] Enable asserts when saving during activity/transition.");
#endif

//////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////// TELEM_SAVETYPE
class MetricTELEM_SAVETYPE : public MetricPlayStat
{
	RL_DECLARE_METRIC(TELEM_SAVETYPE, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

public:
	MetricTELEM_SAVETYPE(const int savetype, const int saveReason, const bool failed, const bool cloud)
	{
		m_SaveType = savetype;
		m_SaveReason = saveReason;
		m_Failed = failed;
		m_Cloud = cloud;
	}

	virtual bool Write(RsonWriter* rw) const {
		return MetricPlayStat::Write(rw) 
			&& rw->WriteInt("a", m_SaveType)
			&& rw->WriteInt("b", m_SaveReason)
			&& rw->WriteBool("c", m_Failed)
			&& rw->WriteBool("d", m_Cloud);
	}

	int m_SaveType;
	int m_SaveReason;
	bool m_Failed;
	bool m_Cloud;
};

void CSaveTelemetryStats::FlushTelemetry(const bool requestFailed, const bool isCloudRequest)
{
	bool sendTelemSavetype = true;
	Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("SEND_METRIC_TELEM_SAVETYPE", 0x016eea50), sendTelemSavetype);
	if (sendTelemSavetype) //Send savegame telemetry.
	{
		if (m_Type != STAT_SAVETYPE_INVALID)
		{
			MetricTELEM_SAVETYPE m(m_Type, m_Reason, requestFailed, isCloudRequest);
			APPEND_METRIC(m);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////// TELEM_SAVETYPE

#if !__FINAL
static mthRandom s_cloudSavesRng;
#endif

CompileTimeAssert(CStatsSavesMgr::TOTAL_NUMBER_OF_FILES == NUM_MULTIPLAYER_STATS_SAVE_SLOTS);

static bool s_FlushProfileStats = false;

class MetricDirtyCloudRead : public MetricPlayStat
{
	RL_DECLARE_METRIC(DR_CLOUD, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

private:
	s64 m_timestamp;

public:
	explicit MetricDirtyCloudRead(const s64 timestamp) : m_timestamp(timestamp) {;}
	virtual bool Write(RsonWriter* rw) const 
	{
		return MetricPlayStat::Write(rw) 
			&& rw->WriteInt64("ts", m_timestamp);
	}
};

class MetricDirtyProfileStatRead : public MetricPlayStat
{
	RL_DECLARE_METRIC(DR_PS, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

private:
	s64 m_timestamp;

public:
	explicit MetricDirtyProfileStatRead(const s64 timestamp) : m_timestamp(timestamp) {;}
	virtual bool Write(RsonWriter* rw) const 
	{
		return MetricPlayStat::Write(rw) 
			&& rw->WriteInt64("ts", m_timestamp);
	}
};

void SPEW_POSIX_TIME( const u64 OUTPUT_ONLY(currentPosixTime), const char* OUTPUT_ONLY(text) )
{
#if !__NO_OUTPUT
	int year = 0;int month = 0;int day = 0;int hour = 0;int min = 0;int sec = 0;
	time_t date = (time_t)currentPosixTime;
	struct tm* ptm;
	ptm = gmtime(&date);

	if (ptm)
	{
		year  = ptm->tm_year+1900;
		month = ptm->tm_mon+1;
		day   = ptm->tm_mday;
		hour  = ptm->tm_hour+1;
		min   = ptm->tm_min;
		sec   = ptm->tm_sec;
		statDebugf1("........ %s ='%" I64FMT "u', date='%d:%d:%d, %d-%d-%d'",text, currentPosixTime,hour,min,sec,day,month,year);
	}
	else
	{
		statDebugf1(" ........ %s = '%" I64FMT "u'", text, currentPosixTime);
	}
#endif
}

RAGE_DEFINE_SUBCHANNEL(stat, savemgr, DIAG_SEVERITY_DEBUG3, DIAG_SEVERITY_DEBUG1)
#undef __stat_channel
#define __stat_channel stat_savemgr

// -------------------- CStatsSaveHistory

bool CStatsSaveHistory::CanSave(const int NOTFINAL_ONLY(file), const eSaveTypes savetype) const
{
	if (savetype == STAT_SAVETYPE_INVALID || savetype == STAT_SAVETYPE_MAX_NUMBER)
		return false;

	// Always instasave a char deletion / immediate flush, no matter what
	if( savetype == STAT_SAVETYPE_DELETE_CHAR || savetype == STAT_SAVETYPE_IMMEDIATE_FLUSH || savetype == STAT_SAVETYPE_COMMERCE_DEPOSIT )
		return true;

#if __ALLOW_LOCAL_MP_STATS_SAVES
	static const u32 MIN_SAVE_TIME_INTERVAL = 10*1000;
#else
	static const u32 MIN_SAVE_TIME_INTERVAL = 30*1000;
#endif

	int minSaveInterval = MIN_SAVE_TIME_INTERVAL;
	Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("CANSAVE_MIN_INTERVAL", 0xb5e2dced), minSaveInterval);

	const u32 currtime = fwTimer::GetSystemTimeInMilliseconds();

	//Check time against same eSaveTypes.
	if (savetype != STAT_SAVETYPE_END_MISSION_CREATOR
		&& savetype != STAT_SAVETYPE_AMBIENT 
		&& savetype != STAT_SAVETYPE_PHOTOS 
		&& savetype != STAT_SAVETYPE_AMB_PROFILE_AWARD_TRACKER
		&& m_records[savetype].m_lastSaveTime>0 
		&& currtime-m_records[savetype].m_lastSaveTime<=((u32)minSaveInterval))
	{
		scriptAssertf(0, "%s:STAT_SAVE - SAVE requested for file=%d, savetype=%s - FAILED. Because time from last save is %d less than the minimum interval %d."
						 ,CTheScripts::GetCurrentScriptNameAndProgramCounter()
						 ,file, GET_STAT_SAVETYPE_NAME(savetype)
						 ,currtime-m_records[savetype].m_lastSaveTime, ((u32)minSaveInterval));

		NOTFINAL_ONLY(statErrorf("Cloud storage:  SAVE requested for file=%d, savetype=%s - FAILED. (last save less than the minimum interval)", file, GET_STAT_SAVETYPE_NAME(savetype));)
		return false;
	}

#if !__FINAL

#if __ALLOW_LOCAL_MP_STATS_SAVES
	static const u32 MIN_SAVE_TIME_INTERVAL_END_MATCH = 20*1000;
#else
	static const u32 MIN_SAVE_TIME_INTERVAL_END_MATCH = 40*1000;
#endif

	int minSaveIntervalEndMatch = MIN_SAVE_TIME_INTERVAL_END_MATCH;
	Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("CANSAVE_MIN_INTERVAL_END_MATCH", 0xdde9468f), minSaveIntervalEndMatch);

	if (STAT_SAVETYPE_END_SESSION != savetype)
	{
		if (m_records[STAT_SAVETYPE_END_MATCH].m_lastSaveTime>0 
			&& currtime-m_records[STAT_SAVETYPE_END_MATCH].m_lastSaveTime<=((u32)minSaveIntervalEndMatch))
		{
			scriptAssertf(0, "%s:STAT_SAVE - SAVE requested for file=%d, savetype=%s - FAILED, because STAT_SAVETYPE_END_MATCH last save is %d less than the minimum interval %d"
				,CTheScripts::GetCurrentScriptNameAndProgramCounter()
				,file, GET_STAT_SAVETYPE_NAME(savetype)
				,currtime-m_records[STAT_SAVETYPE_END_MATCH].m_lastSaveTime, ((u32)minSaveIntervalEndMatch));

			statErrorf("Cloud storage:  SAVE requested for file=%d, savetype=%s - FAILED. (last STAT_SAVETYPE_END_MATCH less than the minimum interval)", file, GET_STAT_SAVETYPE_NAME(savetype));
			return false;
		}
	}

	if (m_records[STAT_SAVETYPE_END_SESSION].m_lastSaveTime>0 
		&& currtime-m_records[STAT_SAVETYPE_END_SESSION].m_lastSaveTime<=((u32)minSaveIntervalEndMatch))
	{
		scriptAssertf(0, "%s:STAT_SAVE - SAVE requested for file=%d, savetype=%s - FAILED, because STAT_SAVETYPE_END_SESSION last save is %d less than the minimum interval %d"
			,CTheScripts::GetCurrentScriptNameAndProgramCounter()
			,file, GET_STAT_SAVETYPE_NAME(savetype)
			,currtime-m_records[STAT_SAVETYPE_END_SESSION].m_lastSaveTime, ((u32)minSaveIntervalEndMatch));

		statErrorf("Cloud storage:  SAVE requested for file=%d, savetype=%s - FAILED. (last STAT_SAVETYPE_END_SESSION less than the minimum interval)", file, GET_STAT_SAVETYPE_NAME(savetype));
		return false;
	}

#endif // !__FINAL

#if __ALLOW_LOCAL_MP_STATS_SAVES
	static const u32 MIN_SAVE_TIME_INTERVAL_START_SESSION = 20*1000;
#else
	static const u32 MIN_SAVE_TIME_INTERVAL_START_SESSION = 40*1000;
#endif

	int minSaveIntervalStartSession = MIN_SAVE_TIME_INTERVAL_START_SESSION;
	Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("CANSAVE_MIN_INTERVAL_START_SESSION", 0x2746f763), minSaveIntervalStartSession);

	if (savetype == STAT_SAVETYPE_START_SESSION || savetype == STAT_SAVETYPE_END_GAMER_SETUP)
	{
		if (m_records[STAT_SAVETYPE_END_CREATE_NEWCHAR].m_lastSaveTime>0 
			&& currtime-m_records[STAT_SAVETYPE_END_CREATE_NEWCHAR].m_lastSaveTime<=((u32)minSaveIntervalStartSession))
		{
			scriptAssertf(0, "%s:STAT_SAVE - SAVE requested for file=%d, savetype=%s - FAILED, because STAT_SAVETYPE_END_CREATE_NEWCHAR last save is %d less than the minimum interval %d."
				,CTheScripts::GetCurrentScriptNameAndProgramCounter()
				,file, GET_STAT_SAVETYPE_NAME(savetype)
				,currtime-m_records[STAT_SAVETYPE_END_CREATE_NEWCHAR].m_lastSaveTime, ((u32)minSaveIntervalStartSession));

			NOTFINAL_ONLY(statErrorf("Cloud storage: savetype=%s - FAILED.", GET_STAT_SAVETYPE_NAME(savetype));)
			return false;
		}

		if (m_records[STAT_SAVETYPE_END_GAMER_SETUP].m_lastSaveTime>0 
			&& currtime-m_records[STAT_SAVETYPE_END_GAMER_SETUP].m_lastSaveTime<=((u32)minSaveIntervalStartSession))
		{
			scriptAssertf(0, "%s:STAT_SAVE - SAVE requested for file=%d, savetype=%s - FAILED, because STAT_SAVETYPE_END_GAMER_SETUP last save is %d less than the minimum interval %d."
				,CTheScripts::GetCurrentScriptNameAndProgramCounter()
				,file, GET_STAT_SAVETYPE_NAME(savetype)
				,currtime-m_records[STAT_SAVETYPE_END_GAMER_SETUP].m_lastSaveTime, ((u32)minSaveIntervalStartSession));

			NOTFINAL_ONLY(statErrorf("Cloud storage:  SAVE requested for file=%d, savetype=%s - FAILED. (last STAT_SAVETYPE_END_GAMER_SETUP less than the minimum interval)", file, GET_STAT_SAVETYPE_NAME(savetype));)
			return false;
		}
	}

	//During an activity we don't care about saving unless script enabled it.
	if (CNetwork::GetNetworkSession().IsActivitySession())
	{
		if (!m_records[savetype].m_canSaveDuringJob)
		{
#if !__FINAL 
			if (PARAM_assertOnCanSaveFail.Get() && savetype != STAT_SAVETYPE_AMBIENT && savetype != STAT_SAVETYPE_AMB_PROFILE_AWARD_TRACKER && savetype != STAT_SAVETYPE_PHOTOS)
			{
				scriptAssertf(0, "%s:STAT_SAVE - SAVE requested for file=%d, savetype=%s - FAILED, During an activity we don't care about saving unless STAT_SET_OPEN_SAVETYPE_IN_JOB was called."
					,CTheScripts::GetCurrentScriptNameAndProgramCounter()
					,file, GET_STAT_SAVETYPE_NAME(savetype));
			}
#endif

			NOTFINAL_ONLY(statErrorf("Cloud storage:  SAVE requested for file=%d, savetype=%s - FAILED. (save during activity)", file, GET_STAT_SAVETYPE_NAME(savetype));)
			return false;
		}
	}

	//During transition we don't care about saving unless we are ending.
	if (CNetwork::GetNetworkSession().IsTransitionActive())
	{
		if (savetype != STAT_SAVETYPE_END_MATCH 
			&& savetype != STAT_SAVETYPE_END_SESSION 
			&& savetype != STAT_SAVETYPE_END_MISSION 
			&& savetype != STAT_SAVETYPE_PROLOGUE)
		{
#if !__FINAL
			if (PARAM_assertOnCanSaveFail.Get() && savetype != STAT_SAVETYPE_AMBIENT && savetype != STAT_SAVETYPE_AMB_PROFILE_AWARD_TRACKER)
			{
				scriptAssertf(0, "%s:STAT_SAVE - SAVE requested for file=%d, savetype=%s - FAILED, During transition we don't care about saving unless we are ending."
					,CTheScripts::GetCurrentScriptNameAndProgramCounter()
					,file, GET_STAT_SAVETYPE_NAME(savetype));
			}
#endif

			NOTFINAL_ONLY(statErrorf("Cloud storage:  SAVE requested for file=%d, savetype=%s - FAILED (save during transition).", file, GET_STAT_SAVETYPE_NAME(savetype));)
			return false;
		}
	}

	switch(savetype)
	{
	case STAT_SAVETYPE_DEFAULT:
	case STAT_SAVETYPE_CASH:
	case STAT_SAVETYPE_END_ATM:
	case STAT_SAVETYPE_CONTACTS:
	case STAT_SAVETYPE_START_MATCH:
	case STAT_SAVETYPE_WEAPON_DROP:
	case STAT_SAVETYPE_JOIN_SC:
		return false;
		break;

	default:
		break;
	}

	return true;
}

void  CStatsSaveHistory::SaveRequested(const eSaveTypes savetype)
{
	if (savetype == STAT_SAVETYPE_INVALID || savetype == STAT_SAVETYPE_MAX_NUMBER)
		return;

	m_records[savetype].m_lastSaveTime = fwTimer::GetSystemTimeInMilliseconds();

	//We finished an activity or are ending the session
	if (savetype == STAT_SAVETYPE_END_MATCH || savetype == STAT_SAVETYPE_END_SESSION || savetype == STAT_SAVETYPE_END_MISSION || savetype == STAT_SAVETYPE_PROLOGUE)
	{
		ResetAfterJob( );
	}
}

bool CStatsSaveHistory::DeferredCloudSave(const eSaveTypes savetype) const
{
	//We are in UGC no profile stats flush.
	if (CNetwork::GetNetworkSession().IsActivitySession())
		return false;

	//We are in UGC Transition no profile stats flush.
	if (CNetwork::GetNetworkSession().IsTransitionActive())
		return false;

	switch(savetype)
	{
	case STAT_SAVETYPE_STUNTJUMP:
	case STAT_SAVETYPE_AMBIENT:
	case STAT_SAVETYPE_AMB_PROFILE_AWARD_TRACKER:
		return true;
		break;
	default:
		break;
	}

	return false;
}

bool CStatsSaveHistory::DeferredProfileStatsFlush(const eSaveTypes savetype) const
{
	//We are in UGC no profile stats flush.
	if (CNetwork::GetNetworkSession().IsActivitySession())
		return false;

	//We are in UGC Transition no profile stats flush.
	if (CNetwork::GetNetworkSession().IsTransitionActive())
		return false;

	switch(savetype)
	{
	case STAT_SAVETYPE_STUNTJUMP:
	case STAT_SAVETYPE_EXPLOITS:
	case STAT_SAVETYPE_AMBIENT:
	case STAT_SAVETYPE_INTERACTION_MENU:
	case STAT_SAVETYPE_AMB_PROFILE_AWARD_TRACKER:
	case STAT_SAVETYPE_RANKUP:
	case STAT_SAVETYPE_END_MISSION_CREATOR:
	case STAT_SAVETYPE_END_MISSION:
	case STAT_SAVETYPE_PHOTOS:
		return true;
		break;
	default:
		break;
	}

	return false;
};

bool CStatsSaveHistory::RequestProfileStatsFlush(const eSaveTypes savetype) const
{
	//Flush Profile Stats if there where EARN/SPEND transactions with CLIENT cash ON.
	if (s_FlushProfileStats && RequestCloudSave(savetype) && !CloudSaveOnly(savetype))
		return true;

	switch(savetype)
	{
	case STAT_SAVETYPE_PRE_STARTSTORE:
	case STAT_SAVETYPE_END_SESSION:
	case STAT_SAVETYPE_END_MATCH:
	case STAT_SAVETYPE_END_GAMER_SETUP:
	case STAT_SAVETYPE_END_SHOPPING:
	case STAT_SAVETYPE_END_CREATE_NEWCHAR:
	case STAT_SAVETYPE_DELETE_CHAR:
	case STAT_SAVETYPE_IMMEDIATE_FLUSH:
	case STAT_SAVETYPE_COMMERCE_DEPOSIT:
		return true;
		break;
	default:
		break;
	}

	return false;
};

bool CStatsSaveHistory::ProfileStatsFlushOnly(const eSaveTypes savetype) const
{
	switch(savetype)
	{
	case STAT_SAVETYPE_PHOTOS:
	case STAT_SAVETYPE_END_SHOPPING:
	case STAT_SAVETYPE_COMMERCE_DEPOSIT:
		return true;
		break;
	default:
		break;
	}

	return false;
};

bool CStatsSaveHistory::CloudSaveOnly(const eSaveTypes savetype) const
{
	switch(savetype)
	{
	case STAT_SAVETYPE_END_MISSION:
	case STAT_SAVETYPE_PROLOGUE:
	case STAT_SAVETYPE_START_SESSION:
	case STAT_SAVETYPE_INTERACTION_MENU:
	case STAT_SAVETYPE_AMB_PROFILE_AWARD_TRACKER:
	case STAT_SAVETYPE_END_MISSION_CREATOR:
	case STAT_SAVETYPE_STORE:
		return true;
		break;
	default:
		break;
	}

	return false;
};

bool CStatsSaveHistory::RequestCloudSave(const eSaveTypes savetype) const
{
	switch(savetype)
	{
	case STAT_SAVETYPE_CHEATER_CHANGE:
	case STAT_SAVETYPE_END_GARAGE:
	case STAT_SAVETYPE_END_MISSION:
	case STAT_SAVETYPE_SCRIPT_MP_GLOBALS:
	case STAT_SAVETYPE_PROLOGUE:
	case STAT_SAVETYPE_START_SESSION:
	case STAT_SAVETYPE_INTERACTION_MENU:
	case STAT_SAVETYPE_AMB_PROFILE_AWARD_TRACKER:
	case STAT_SAVETYPE_END_MISSION_CREATOR:
	case STAT_SAVETYPE_RANKUP:
	case STAT_SAVETYPE_IMMEDIATE_FLUSH:
	case STAT_SAVETYPE_STORE:
	case STAT_SAVETYPE_PRE_STARTSTORE:
	case STAT_SAVETYPE_END_SESSION:
	case STAT_SAVETYPE_END_MATCH:
	case STAT_SAVETYPE_END_CREATE_NEWCHAR:
	case STAT_SAVETYPE_END_GAMER_SETUP:
	case STAT_SAVETYPE_DELETE_CHAR:
		return true;
		break;

	default:
		break;
	}

	return false;
};


#if __BANK
static int s_BankFailSlot = -1;
static bool s_bankResynchProfileStats = false;

void 
CStatsSavesMgr::Bank_InitWidgets( bkBank& bank )
{
	bank.PushGroup("Savegame", false);
	{
		bank.AddSlider("nompsavesometimes", &m_save.m_SimulateFailPct, 0, 100, 1);
		bank.AddSlider("nomploadsometimes", &m_load.m_SimulateFailPct, 0, 100, 1);

		bank.AddSeparator();

		bank.AddSlider("Command Value - failmploadalways", &s_BankFailSlot, -1, 5, 1);
		bank.AddButton("Set command Value - failmploadalways",    datCallback(MFA(CStatsSavesMgr::Bank_SetFailSlot), (datBase*)this));
		bank.AddButton("Clear command Value - failmploadalways",  datCallback(MFA(CStatsSavesMgr::Bank_ClearFailSlot), (datBase*)this));
		bank.AddToggle("ReSynch Multiplayer Profile Stats on next Game Load", &s_bankResynchProfileStats);
	}
	bank.PopGroup();
}

void 
CStatsSavesMgr::Bank_SetFailSlot()
{
	if (-1 == s_BankFailSlot)
		PARAM_failmploadalways.Set("-1");
	else if (0 == s_BankFailSlot)
		PARAM_failmploadalways.Set("0");
	else if (1 == s_BankFailSlot)
		PARAM_failmploadalways.Set("1");
	else if (2 == s_BankFailSlot)
		PARAM_failmploadalways.Set("2");
	else if (3 == s_BankFailSlot)
		PARAM_failmploadalways.Set("3");
	else if (4 == s_BankFailSlot)
		PARAM_failmploadalways.Set("4");
	else if (5 == s_BankFailSlot)
		PARAM_failmploadalways.Set("5");
}

void 
CStatsSavesMgr::Bank_ClearFailSlot()
{
	PARAM_failmploadalways.Set(NULL);
}

#endif

void
CStatsSavesMgr::Initialize()
{
	if (m_Initialized)
		return;

	m_Initialized = true;

	m_load.Reset();
	m_save.Reset();

#if !__FINAL
	XPARAM(level);
	const char* levelName = NULL;
	if (PARAM_level.Get(levelName))
	{
		if (atHashString(levelName) != atHashString("gta5",0x6D2855E0))
		{
			PARAM_nompsavegame.Set("0");
			PARAM_disableOnlineStatsAreSynched.Set("0");
		}
	}

	if (!PARAM_nompsavesometimes.Get(m_save.m_SimulateFailPct))
	{
		m_save.m_SimulateFailPct = 0;
	}

	if (!PARAM_nomploadsometimes.Get(m_load.m_SimulateFailPct))
	{
		m_load.m_SimulateFailPct = 0;
	}

	if (PARAM_nonetwork.Get() || PARAM_lan.Get() || PARAM_nompsavegame.Get() || PARAM_cloudForceAvailable.Get())
	{
		for (int i=0; i<TOTAL_NUMBER_OF_FILES; i++)
		{
			ClearLoadPending(i);
			StatsInterface::SetAllStatsToSynched(i, true, true);
		}

		CProfileStats& profileStatsMgr = CLiveManager::GetProfileStatsMgr();
		profileStatsMgr.SetSynchronized( CProfileStats::PS_SYNC_MP NOTFINAL_ONLY(, true) );
	}

	int slot = -1;
	if (PARAM_failmpload.Get(slot) || PARAM_failmploadalways.Get(slot))
	{
		if (-1 < slot)
		{
			ClearLoadPending(slot);
			SetLoadFailed(slot, LOAD_FAILED_REASON_FAILED_TO_LOAD);
		}
		else
		{
			for (int i=0; i<TOTAL_NUMBER_OF_FILES; i++)
			{
				ClearLoadPending(i);
				SetLoadFailed(i, LOAD_FAILED_REASON_FAILED_TO_LOAD);
			}
		}
	}

	if (PARAM_nonetwork.Get() || PARAM_lan.Get() || PARAM_nompsavegame.Get() || PARAM_cloudForceAvailable.Get() || PARAM_failmpload.Get() || PARAM_failmploadalways.Get())
	{
		statWarningf("Cloud operations disable:");
		statWarningf("   .... PARAM_nonetwork           = %s", PARAM_nonetwork.Get() ? "True":"False");
		statWarningf("   .... PARAM_lan                 = %s", PARAM_lan.Get() ? "True":"False");
		statWarningf("   .... PARAM_nompsavegame        = %s", PARAM_nompsavegame.Get() ? "True":"False");
		statWarningf("   .... PARAM_cloudForceAvailable = %s", PARAM_cloudForceAvailable.Get() ? "True":"False");
		statWarningf("   .... PARAM_failmpload = %s, %d", PARAM_failmpload.Get() ? "True":"False", slot);
		statWarningf("   .... PARAM_failmploadalways = %s, %d", PARAM_failmploadalways.Get() ? "True":"False", slot);
	}
#endif // !__FINAL

#if __MPCLOUDSAVES_ONLY_ONEFILE
	for (int i=STAT_MP_CATEGORY_CHAR0; i<TOTAL_NUMBER_OF_FILES; i++)
	{
		ClearLoadPending(i);
		ClearLoadFailed(i);
	}
#endif // __MPSAVES_ONLY_ONEFILE

	int minProfileFlush = eSaveOperations::MIN_PROFILESTATS_FLUSH;
	Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("MPSAVE_MIN_PROFILESTATS_FLUSH", 0x8d400e25), minProfileFlush);
	m_save.m_minForceFlushTime = (u32)minProfileFlush;

	int delaySaveFailed = eSaveOperations::DELAY_NEXT_SAVE;
	Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("MPSAVE_DELAY_NEXT_SAVE", 0x53e403b9), delaySaveFailed);
	m_save.m_delaySaveFailed = (u32)delaySaveFailed;

	m_DirtyProfileStatsReadDetected = false;
	m_DirtyCloudReadDetected = false;
	m_DirtyReadServerTime = 0;
	m_DirtyReadCloudFile = -1;
}

void
CStatsSavesMgr::Init(const unsigned initMode)
{
	if(INIT_CORE == initMode)
	{
		statAssert(MEM_CARD_BUSY != CGenericGameStorage::GetMpStatsLoadStatus());
		statAssert(!IsLoadInProgress(STAT_INVALID_SLOT));

		statAssert(MEM_CARD_BUSY != CGenericGameStorage::GetMpStatsSaveStatus());
		statAssert(!IsSaveInProgress());
	}
	else if(INIT_AFTER_MAP_LOADED == initMode)
	{
	}
	else if (INIT_SESSION == initMode)
	{
		statAssert( MEM_CARD_BUSY != CGenericGameStorage::GetMpStatsSaveStatus() );
		statAssert(!IsSaveInProgress());

		Initialize();
	}
}

void
CStatsSavesMgr::Shutdown(unsigned shutdownMode)
{
	if (!m_Initialized)
		return;

	if(shutdownMode == SHUTDOWN_CORE || shutdownMode == SHUTDOWN_SESSION)
	{
		SignedOffline();
		m_Initialized = false;
	}
	else if(shutdownMode == SHUTDOWN_WITH_MAP_LOADED)
	{
	}
}

void
CStatsSavesMgr::SignedOffline()
{
	statDebugf1("SignedOffline - m_Initialized=%s", m_Initialized?"true":"false");

	if (m_Initialized)
	{
		statDebugf1("SignedOffline - m_save.m_InProgress=%s", m_save.m_InProgress?"true":"false");
		statDebugf1("SignedOffline - m_load.m_InProgress=%s", m_load.m_InProgress?"true":"false");

		if (m_save.m_InProgress && (MEM_CARD_BUSY == CGenericGameStorage::GetMpStatsSaveStatus()))
			CGenericGameStorage::CancelMpStatsSave();

		if (m_load.m_InProgress && (MEM_CARD_BUSY == CGenericGameStorage::GetMpStatsLoadStatus()))
			CGenericGameStorage::CancelMpStatsLoad();

		m_save.Shutdown();
		m_load.Shutdown();
	}
}

void
CStatsSavesMgr::Update()
{
	// No network
	if (PARAM_nonetwork.Get() || PARAM_lan.Get() || PARAM_nompsavegame.Get() || PARAM_cloudForceAvailable.Get())
		return;

	//Only load if the player is signed in.
	if (!NetworkInterface::IsLocalPlayerSignedIn())
		return;

	//Cloud Storage
	UpdateCloudStorage( );
}

bool
CStatsSavesMgr::CheckForProfileStatsDirtyRead( )
{
	statDebugf1(" Check For Profile Stats Dirty Read ");

	bool dirtyRead = false;

	//Set Local value for last MP flush.
	CProfileSettings& settings = CProfileSettings::GetInstance();
	if(!settings.AreSettingsValid())
	{
		statDebugf1("Abort CheckForProfileStatsDirtyRead, Profile settings not valid, probably a login or logout in the same frame.");
		return true;
	}

	u64 low32 = 0;
	if(settings.Exists(CProfileSettings::MP_FLUSH_POSIXTIME_LOW32))
		low32 = static_cast< u64 >( settings.GetInt(CProfileSettings::MP_FLUSH_POSIXTIME_LOW32) );

	u64 high32 = 0;
	if(settings.Exists(CProfileSettings::MP_FLUSH_POSIXTIME_HIGH32))
		high32 = static_cast< u64 >( settings.GetInt(CProfileSettings::MP_FLUSH_POSIXTIME_HIGH32) );

	u64 localflushPosixTime = high32 << 32;
	localflushPosixTime = localflushPosixTime | low32;

#if !__FINAL
	int overrideTime = 0;
	if (PARAM_changeLocalProfileTime.Get(overrideTime))
	{
		//Change the sign of overrideTime
		const int nosignvalue = (-1 * overrideTime);

		if (overrideTime > 0)
			localflushPosixTime += (u64)overrideTime;
		else if (overrideTime < 0 && localflushPosixTime > (u64)(nosignvalue))
			localflushPosixTime -= (u64)(nosignvalue);
	}
#endif //!__FINAL

	const u64 flushPosixTime = static_cast< u64 >( StatsInterface::GetInt64Stat( StatId("PROFILE_STATS_LAST_FLUSH") ) );

	m_DirtyProfileStatsReadDetected = false;

	//Dirty read detected
	if (localflushPosixTime > flushPosixTime)
	{
		const u64 currentPosixTime = rlGetPosixTime();

		statDebugf1(" .... Dirty Profile Stats read detected: ");
		SPEW_POSIX_TIME( localflushPosixTime, "Local Time" );
		SPEW_POSIX_TIME( flushPosixTime, "Server Time" );
		SPEW_POSIX_TIME( currentPosixTime, "Current Time" );
		NOTFINAL_ONLY(statDebugf1("........ Override local Time = '%d'", overrideTime);)

		//4 hours max wait time.
		int timeoutPeriod = 14400;
		Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MP_DIRTY_READ_TIMEOUT_SECONDS", 0x5854f55c), timeoutPeriod);
		NOTFINAL_ONLY( bool overrideTimeoutPeriod = PARAM_dirtyreadtimeoutPeriod.Get(timeoutPeriod); )

		s64 diff = currentPosixTime - (localflushPosixTime+timeoutPeriod);
		statDebugf1("........ Timeout='%d', Diff='%" I64FMT "d'", timeoutPeriod, diff);

		//Only set to failed due to dirty reads if we are still under a certain time period.
		dirtyRead = (diff < 0);

		NOTFINAL_ONLY( if ( overrideTimeoutPeriod && timeoutPeriod == 0) )
			NOTFINAL_ONLY(	 dirtyRead = false; )
	}

	if (dirtyRead)
	{
		m_DirtyProfileStatsReadDetected = true;
		m_DirtyReadServerTime = flushPosixTime;

		MetricDirtyProfileStatRead m((s64)localflushPosixTime);
		CNetworkTelemetry::AppendMetric(m);

		statWarningf(" Dirty Profile Stats read detected and set to true. ");
	}

	return dirtyRead;
}

u64
CStatsSavesMgr::GetLocalCloudDirtyReadValue( const u32 file )
{
	u64 low32 = 0;
	u64 high32 = 0;

	//Set Local value for last MP flush.
	CProfileSettings& settings = CProfileSettings::GetInstance();
	statAssert( settings.AreSettingsValid() );

	switch ( file )
	{
	case STAT_MP_CATEGORY_CHAR0:
		{
			if(settings.Exists(CProfileSettings::MP_CLOUD0_POSIXTIME_LOW32))
				low32 = static_cast< u64 >( settings.GetInt(CProfileSettings::MP_CLOUD0_POSIXTIME_LOW32) );
			if(settings.Exists(CProfileSettings::MP_CLOUD0_POSIXTIME_HIGH32))
				high32 = static_cast< u64 >( settings.GetInt(CProfileSettings::MP_CLOUD0_POSIXTIME_HIGH32) );
		}
		break;

	case STAT_MP_CATEGORY_CHAR1:
		{
			if(settings.Exists(CProfileSettings::MP_CLOUD1_POSIXTIME_LOW32))
				low32 = static_cast< u64 >( settings.GetInt(CProfileSettings::MP_CLOUD1_POSIXTIME_LOW32) );
			if(settings.Exists(CProfileSettings::MP_CLOUD1_POSIXTIME_HIGH32))
				high32 = static_cast< u64 >( settings.GetInt(CProfileSettings::MP_CLOUD1_POSIXTIME_HIGH32) );
		}
		break;

	case STAT_MP_CATEGORY_DEFAULT:
		{
			if(settings.Exists(CProfileSettings::MP_CLOUD_POSIXTIME_LOW32))
				low32 = static_cast< u64 >( settings.GetInt(CProfileSettings::MP_CLOUD_POSIXTIME_LOW32) );
			if(settings.Exists(CProfileSettings::MP_CLOUD_POSIXTIME_HIGH32))
				high32 = static_cast< u64 >( settings.GetInt(CProfileSettings::MP_CLOUD_POSIXTIME_HIGH32) );
		}
		break;
	}

	u64 result = high32 << 32;
	result = result | low32;

	return result;
}

void
CStatsSavesMgr::ClearLocalDirtyReadProfileSettings()
{
	CProfileSettings& settings = CProfileSettings::GetInstance();
	if (!settings.AreSettingsValid())
	{
		statDebugf1("Abort ClearLocalDirtyReadProfileSettings, settings are not valid.");
		return;
	}

	statWarningf(" **** Clear local Dirty Read profile settings **** ");
	settings.Set(CProfileSettings::MP_FLUSH_POSIXTIME_LOW32, 0);
	settings.Set(CProfileSettings::MP_FLUSH_POSIXTIME_HIGH32, 0);
	settings.Set(CProfileSettings::MP_CLOUD0_POSIXTIME_LOW32, 0);
	settings.Set(CProfileSettings::MP_CLOUD0_POSIXTIME_HIGH32, 0);
	settings.Set(CProfileSettings::MP_CLOUD1_POSIXTIME_LOW32, 0);
	settings.Set(CProfileSettings::MP_CLOUD1_POSIXTIME_HIGH32, 0);
	settings.Set(CProfileSettings::MP_CLOUD_POSIXTIME_LOW32, 0);
	settings.Set(CProfileSettings::MP_CLOUD_POSIXTIME_HIGH32, 0);

	settings.Write(true);
}

bool
CStatsSavesMgr::CheckForCloudDirtyRead( const u32 file )
{
	statDebugf1(" Check For Cloud Dirty Read ");

	CProfileSettings& settings = CProfileSettings::GetInstance();
	if(!settings.AreSettingsValid())
	{
		statDebugf1("Abort CheckForProfileStatsDirtyRead, Profile settings not valid, probably a login or logout in the same frame.");
		return true;
	}

	bool dirtyRead = false;

	u64 localCloudPosixTime = GetLocalCloudDirtyReadValue( file );

#if !__FINAL
	int overrideTime = 0;
	if (PARAM_changeLocalCloudTime.Get(overrideTime))
	{
		//Change the sign of overrideTime
		const int nosignvalue = (-1 * overrideTime);

		if (overrideTime > 0)
			localCloudPosixTime += (u64)overrideTime;
		else if (overrideTime < 0 && localCloudPosixTime > (u64)(nosignvalue))
			localCloudPosixTime -= (u64)(nosignvalue);
	}
#endif //!__FINAL

	//Check timestamp for Multi Player
	const char* statName = "_SaveMpTimestamp_0";
	switch ( file )
	{
	case STAT_MP_CATEGORY_DEFAULT: statName = "_SaveMpTimestamp_0"; break;
	case STAT_MP_CATEGORY_CHAR0:   statName = "_SaveMpTimestamp_1"; break;
	case STAT_MP_CATEGORY_CHAR1:   statName = "_SaveMpTimestamp_2"; break;
	}
	StatId statTimestamp(statName);

	const u64 cloudFilePosixTime = StatsInterface::GetUInt64Stat( statTimestamp );

	m_DirtyCloudReadDetected = false;
	m_DirtyReadCloudFile  = -1;

	//Dirty read detected
	if (localCloudPosixTime > cloudFilePosixTime)
	{
		const u64 currentPosixTime = rlGetPosixTime();

		statDebugf1(" .... Dirty Cloud read detected: ");
		SPEW_POSIX_TIME( localCloudPosixTime, "Local Time" );
		SPEW_POSIX_TIME( cloudFilePosixTime, "Server Time" );
		SPEW_POSIX_TIME( currentPosixTime, "Current Time" );
		NOTFINAL_ONLY(statDebugf1("........ Override local Time = '%d'", overrideTime);)

		//4 hours max wait time.
		int timeoutPeriod = 14400;
		Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MP_DIRTY_READ_TIMEOUT_SECONDS", 0x5854f55c), timeoutPeriod);
		NOTFINAL_ONLY( bool overrideTimeoutPeriod = PARAM_dirtycloudreadtimeoutPeriod.Get( timeoutPeriod ); )

		s64 diff = currentPosixTime - (localCloudPosixTime+timeoutPeriod);
		statDebugf1("........ Timeout='%d', Diff='%" I64FMT "d'", timeoutPeriod, diff);

		//Only set to failed due to dirty reads if we are still under a certain time period.
		dirtyRead = (diff < 0);

		NOTFINAL_ONLY( if ( overrideTimeoutPeriod && timeoutPeriod == 0) )
			NOTFINAL_ONLY(	 dirtyRead = false; )
	}

	if (dirtyRead)
	{
		m_DirtyCloudReadDetected = true;
		m_DirtyReadCloudFile = file;
		m_DirtyReadServerTime = cloudFilePosixTime;

		MetricDirtyCloudRead m((s64)localCloudPosixTime);
		CNetworkTelemetry::AppendMetric(m);

		statWarningf(" Dirty Cloud read detected and set to true. ");
	}

	return dirtyRead;
}

void
CStatsSavesMgr::UpdateCloudStorage( )
{
	if (!NetworkInterface::IsCloudAvailable())
		return;

	if (!NetworkInterface::HasValidRockstarId() && StatsInterface::GetCloudSavegameService() == RL_CLOUD_ONLINE_SERVICE_SC)
		return;

	if (!CLiveManager::CheckOnlinePrivileges())
		return;

	//Loading Profile stats...
	if (m_SynchProfileStats)
	{
		CProfileStats& profileStatsMgr = CLiveManager::GetProfileStatsMgr();
		m_SynchProfileStats = profileStatsMgr.SynchronizePending(CProfileStats::PS_SYNC_MP);

		if (!m_SynchProfileStats)
		{
            CSavingMessage::Clear();
			statDebugf1("Cloud storage: Profile Stats LOAD Ended.");

			int errorCode = 0;
			if ( profileStatsMgr.SynchronizedFailed(CProfileStats::PS_SYNC_MP, errorCode) )
			{
				statErrorf("Cloud storage: Profile Stats LOAD Failed.");

				APPEND_SERVICE_FAILURE(m_LoadUid, MetricServiceFailed::PS_HTTP_FAIL, errorCode)

				ClearLoadPending(STAT_MP_CATEGORY_DEFAULT);
				SetLoadFailed(STAT_MP_CATEGORY_DEFAULT, LOAD_FAILED_REASON_FAILED_TO_LOAD);
				m_load.m_OverrideSlot = m_load.m_CurrentSlot = STAT_MP_CATEGORY_DEFAULT;
				profileStatsMgr.ForceMPProfileStatsGetFromCloud();
				return;
			}
			else if ( CheckForProfileStatsDirtyRead( ) )
			{
				statErrorf("Cloud storage: Profile Stats DIRTY READ Detected.");

				APPEND_SERVICE_FAILURE(m_LoadUid, MetricServiceFailed::PS_DIRTY_READ, 0)

				ClearLoadPending(STAT_MP_CATEGORY_DEFAULT);
				SetLoadFailed(STAT_MP_CATEGORY_DEFAULT, LOAD_FAILED_REASON_DIRTY_PROFILE_STAT_READ);
				m_load.m_OverrideSlot = m_load.m_CurrentSlot = STAT_MP_CATEGORY_DEFAULT;
				profileStatsMgr.ForceMPProfileStatsGetFromCloud();
				return;
			}
			else
			{
				m_load.m_OverrideSlot = m_load.m_CurrentSlot = STAT_MP_CATEGORY_DEFAULT;
			}
		}
	}

	//Loading...
	if (m_load.m_InProgress)
	{
		MemoryCardError status = CGenericGameStorage::GetMpStatsLoadStatus();

		m_load.m_InProgress = (MEM_CARD_BUSY == status);

		if (!m_load.m_InProgress)
		{
			statDebugf1("Cloud storage: LOAD finished for category=%d.", m_load.m_CurrentSlot);

			CSavingMessage::Clear();

			ClearLoadPending(m_load.m_CurrentSlot);

			eReasonForFailureToLoadSavegame failureCode = CGenericGameStorage::GetReasonForFailureToLoadSavegame();

#if !__FINAL
			if (MEM_CARD_COMPLETE == status)
			{
				bool forceFail = false;
				int slot = -1;
				if (PARAM_failmploadalways.Get(slot))
				{
					forceFail = (slot == -1 || slot == (int)m_load.m_CurrentSlot);
				}

				if(!forceFail && m_load.m_SimulateFailPct)
				{
					statDebugf1("Cloud storage: LOAD finished - Simulating failure %d%% of the time...", m_load.m_SimulateFailPct);
					if(s_cloudSavesRng.GetRanged(1, 100) <= m_load.m_SimulateFailPct)
					{
						statDebugf1("Cloud storage: LOAD finished - Failure simulated!");
						forceFail   = true;
						status      = MEM_CARD_ERROR;
						failureCode = LOAD_FAILED_REASON_FILE_CORRUPT;
					}
					else
					{
						statDebugf1("Cloud storage: LOAD finished - Failure averted!");
					}
				}
			}
#endif // !__FINAL

			//Check for error file not found.
			if (MEM_CARD_ERROR == status && LOAD_FAILED_REASON_FILE_NOT_FOUND == failureCode)
			{
				switch (m_load.m_CurrentSlot)
				{
					case STAT_MP_CATEGORY_DEFAULT:
					{
						status = MEM_CARD_COMPLETE;
						statDebugf1("Cloud storage: LOAD finished - Clear DEFAULT error code LOAD_FAILED_REASON_FILE_NOT_FOUND!");
						ClearLocalDirtyReadProfileSettings();
					}
					break;

					case STAT_MP_CATEGORY_CHAR0:
					{
						if (!StatsInterface::GetBooleanStat(STAT_MP0_CHAR_ISACTIVE))
						{
							status = MEM_CARD_COMPLETE;
							statDebugf1("Cloud storage: LOAD finished - Clear MP0 error code LOAD_FAILED_REASON_FILE_NOT_FOUND!");
						}
					}
					break;

					case STAT_MP_CATEGORY_CHAR1: 
					{
						if (!StatsInterface::GetBooleanStat(STAT_MP1_CHAR_ISACTIVE))
						{
							status = MEM_CARD_COMPLETE;
							statDebugf1("Cloud storage: LOAD finished - Clear MP1 error code LOAD_FAILED_REASON_FILE_NOT_FOUND!");
						}
					}
					break;

				default:
					break;
				}

				if (status == MEM_CARD_COMPLETE)
				{
					//CGenericGameStorage::SetReasonForFailureToLoadSavegame( LOAD_FAILED_REASON_NONE );
					failureCode = LOAD_FAILED_REASON_NONE;
				}
			}

			//Check for Dirty Reads
			if ( MEM_CARD_ERROR != status && CheckForCloudDirtyRead( m_load.m_CurrentSlot ) )
			{
				statErrorf("Cloud storage: Cloud Dirty Read Detected on slot='%d'", m_load.m_CurrentSlot);
				failureCode = LOAD_FAILED_REASON_DIRTY_CLOUD_READ;
				status = MEM_CARD_ERROR;
			}

			//Failed to load the cload savegame
			if (MEM_CARD_ERROR == status)
			{
				APPEND_SERVICE_FAILURE(m_LoadUid, failureCode, CGenericGameStorage::GetCloudSaveResultcode())

				statWarningf("Cloud storage: LOAD finished - FAILED.");

				SetLoadFailed(m_load.m_CurrentSlot, failureCode);

				CGenericGameStorage::ClearMpStatsLoadStatus();
			}

#if __MPCLOUDSAVES_ONLY_ONEFILE
			for (int i=STAT_MP_CATEGORY_CHAR0; i<TOTAL_NUMBER_OF_FILES; i++)
			{
				ClearLoadPending(i);
				ClearLoadFailed(i);
			}
#endif // __MPSAVES_ONLY_ONEFILE

#if !__FINAL
			if (STAT_MP_CATEGORY_DEFAULT == m_load.m_CurrentSlot && !CloudLoadFailed(STAT_MP_CATEGORY_DEFAULT))
			{
				int numActiveSlots = 0;
				if(PARAM_mpactiveslots.Get(numActiveSlots))
				{
					if (numActiveSlots > 1)
					{
						StatsInterface::SetStatData(STAT_MP0_CHAR_ISACTIVE, true); 
						StatsInterface::SetStatData(STAT_MP1_CHAR_ISACTIVE, true);
					}
				}

				int activeChar = 0;
				if(PARAM_mpactivechar.Get(activeChar))
				{
					int activeSlot = activeChar+1;

					if (statVerify(STAT_MP_CATEGORY_DEFAULT < activeSlot) && statVerify(activeSlot < MAX_NUM_MP_CHARS))
					{
						StatsInterface::SetStatData(STAT_MPPLY_LAST_MP_CHAR, activeChar);
						switch (activeSlot)
						{
						case STAT_MP_CATEGORY_CHAR0: StatsInterface::SetStatData(STAT_MP0_CHAR_ISACTIVE, true); break;
						case STAT_MP_CATEGORY_CHAR1: StatsInterface::SetStatData(STAT_MP1_CHAR_ISACTIVE, true); break;
						}
					}
				}
			}
#endif

			//Load 2nd file - current character selected slot.
			if (STAT_MP_CATEGORY_DEFAULT == m_load.m_CurrentSlot && !CloudLoadFailed(STAT_MP_CATEGORY_DEFAULT) && QueueCheckCloudFileLoadPending())
			{
				// -------- Do nothing carry on with load.
				QueueLoad(STATS_LOAD_CLOUD);
			}
			else
			{
				m_load.m_OverrideSlot = m_load.m_CurrentSlot = STAT_MP_CATEGORY_DEFAULT;
			}

			//Now that 2nd file has completed, sync profile stats if we need to.
			if (!m_load.m_InProgress && !IsLoadPending(STAT_MP_CATEGORY_DEFAULT) && !DirtyCloudReadDetected( ))
			{
				CProfileStats& profileStatsMgr = CLiveManager::GetProfileStatsMgr();

#if __BANK
				if (s_bankResynchProfileStats)
				{
					s_bankResynchProfileStats = false;

					if (profileStatsMgr.SynchronizePending(CProfileStats::PS_SYNC_MP))
						profileStatsMgr.CanSynchronize( true );

					if (profileStatsMgr.Synchronized(CProfileStats::PS_SYNC_MP))
						profileStatsMgr.ClearSynchronized( CProfileStats::PS_SYNC_MP );
				}
#endif // __BANK

				if (!profileStatsMgr.SynchronizePending(CProfileStats::PS_SYNC_MP) && !profileStatsMgr.Synchronized(CProfileStats::PS_SYNC_MP))
				{
					if (!statVerify(profileStatsMgr.CanSynchronize( true )))
					{
						m_SynchProfileStats = false;
					}
					else
					{
						m_SynchProfileStats = profileStatsMgr.Synchronize(true, true);
					}

					statAssertf(m_SynchProfileStats, "Failed to start synch profile of profile stats");
					if (!m_SynchProfileStats)
					{
						ClearLoadPending(m_load.m_CurrentSlot);
						SetLoadFailed(m_load.m_CurrentSlot, LOAD_FAILED_REASON_FAILED_TO_LOAD);
					}
					else
					{
						CSavingMessage::BeginDisplaying( CSavingMessage::STORAGE_DEVICE_MESSAGE_CLOUD_LOADING );
						statDebugf1("Cloud storage: Profile Stats LOAD Started");
					}
				}
			}
		}
	}

	//Saving...
	else if (m_save.m_InProgress)
	{
		const MemoryCardError status = CGenericGameStorage::GetMpStatsSaveStatus();
		m_save.m_InProgress = (MEM_CARD_BUSY == status);

		if (!m_save.m_InProgress)
		{
			CSavingMessage::Clear();

			bool saveFailed = (MEM_CARD_ERROR == status);
			//Retry save for 408, 429, and 504.
			int resultcode = CGenericGameStorage::GetCloudSaveResultcode();

#if !__FINAL
			if(!saveFailed && m_save.m_SimulateFailPct)
			{
				statDebugf1("Cloud storage: SAVE finished - Simulating failure %d%% of the time...", m_save.m_SimulateFailPct);
				if(s_cloudSavesRng.GetRanged(1, 100) <= m_save.m_SimulateFailPct)
				{
					statDebugf1("Cloud storage: SAVE finished - Failure simulated!");
					saveFailed = true;
					resultcode = HTTP_CODE_REQUESTTIMEOUT;
				}
				else
				{
					statDebugf1("Cloud storage: SAVE finished - Failure averted!");
				}
			}
#endif // !__FINAL

			CSaveTelemetryStats cachedSaveTelemetry;
			cachedSaveTelemetry = m_save.m_InProgressSaveTelemetry;

			if (saveFailed)
			{
				//Send savegame telemetry.
				m_save.m_InProgressSaveTelemetry.FlushTelemetry(true, true);

				SetSaveFailed(m_save.m_CurrentSlot, resultcode);

				if (m_save.m_RetryFailedSave<eSaveOperations::MAX_NUMBER_RETRY_SAVE && RetrySaveOnHttpErrorCode(resultcode))
				{
					m_save.m_RetryFailedSave += 1;
					m_save.m_NextSaveTime = sysTimer::GetSystemMsTime() + (eSaveOperations::DELAY_FAILED_SAVE*m_save.m_RetryFailedSave);
					m_save.m_RequestSlot |= (1<<m_save.m_CurrentSlot);

					if (m_save.m_CurrentSlotIsCritical)
					{
						m_save.m_IsCritical |= (1<<m_save.m_CurrentSlot);
					}

					statWarningf("Cloud storage: SAVE finished - FAILED, result code is %d, retry save in %ds.", resultcode, (eSaveOperations::DELAY_FAILED_SAVE*m_save.m_RetryFailedSave)/1000);
				}
				else
				{
					statAssertf(0, "Cloud storage: SAVE finished - FAILED, result code is %d", resultcode);

					//We are not going to retry any more saves so clear the Critical.
					m_save.m_CurrentSlotIsCritical = false;
				}

				m_save.m_CurrentSlot = STAT_MP_CATEGORY_DEFAULT;

				CGenericGameStorage::ClearMpStatsSaveStatus();
			}
			else
			{
				statDebugf1("Cloud storage: SAVE finished.");

				//Send savegame telemetry.
				if (STAT_MP_CATEGORY_DEFAULT < m_save.m_CurrentSlot)
				{
					m_save.m_InProgressSaveTelemetry.FlushTelemetry(false, true);
				}

				//Clear Failed Save
				ClearSaveFailed(m_save.m_CurrentSlot);

				//Reset save retries.
				m_save.m_RetryFailedSave = 0;

				//Only clear the critical if it was successful.
				m_save.m_CurrentSlotIsCritical = false;

				//Set local slot Dirty Read timestamp.
				SaveLocalCloudDirtyReadTimestamp();
			}

#if !__MPCLOUDSAVES_ONLY_ONEFILE
			//Save 2nd file - current character selected slot.
			if (!saveFailed && STAT_MP_CATEGORY_DEFAULT == m_save.m_CurrentSlot)
			{
				SetupSavegameSlot(true);
				QueueSave(STATS_SAVE_CLOUD);
			}
			else
#endif // __MPSAVES_ONLY_ONEFILE
			{
				m_save.m_OverrideSlot = m_save.m_CurrentSlot = STAT_MP_CATEGORY_DEFAULT;

				if(!saveFailed && PendingSaveRequests())
				{
					const int slot = GetNextPendingSaveRequest();
					if (STAT_INVALID_SLOT < slot)
					{
						statDebugf1("Cloud storage: SAVE request pending for file %d.", slot);
						m_save.m_CurrentSlot = slot;
						QueueSave(STATS_SAVE_CLOUD);
					}
				}
				else if (!saveFailed && (m_save.m_flushProfileStat))
				{
#if __ASSERT
					const u32 currentTime = fwTimer::GetSystemTimeInMilliseconds();
					if (currentTime-m_save.m_lastFlushProfileStatTime <= m_save.m_minForceFlushTime && !m_save.m_lastFlushProfileStatWasDeferred)
					{
						statAssertf(!m_save.m_IsCritical, "Profile stats being flushed in the again, time since last flush=%d, min flush is %d seconds", currentTime-m_save.m_lastFlushProfileStatTime, m_save.m_minForceFlushTime/1000);
					}

					statDebugf1("Cloud storage: SAVE Performing a profile stats FLUSH - Time since last flush %d.", currentTime - m_save.m_lastFlushProfileStatTime);
#endif //__ASSERT

					TriggerProfileStatsFlush(cachedSaveTelemetry);
				}
			}
		}
	}

	// Idle Mode...
	else
	{
		const u32 currtime = sysTimer::GetSystemMsTime();

		//Check if there are any pending saves.
		if (m_save.m_RequestSlot!=0 && m_save.m_NextSaveTime<currtime)
		{
			if (PendingSaveRequests())
			{
				const int slot = GetNextPendingSaveRequest();

				if (STAT_INVALID_SLOT < slot)
				{
					statDebugf1("Cloud storage: SAVE request pending for file %d.", slot);
					m_save.m_CurrentSlot = slot;
					QueueSave(STATS_SAVE_CLOUD);
				}
			}
			//Delay further the save
			else
			{
				if (m_save.m_RetryFailedSave > 0)
				{
					m_save.m_NextSaveTime = currtime + (eSaveOperations::DELAY_FAILED_SAVE*m_save.m_RetryFailedSave);
					statDebugf1("Cloud storage: Delay SAVE request further time=%ds.", (eSaveOperations::DELAY_FAILED_SAVE*m_save.m_RetryFailedSave)/1000);
				}
				else
				{
					m_save.m_NextSaveTime = currtime + m_save.m_delaySaveFailed;
					statDebugf1("Cloud storage: Delay SAVE request further time=%ds.", m_save.m_delaySaveFailed/1000);
				}
			}
		}

		//We only have a DEFERRED Cloud save pending.
		if (m_save.m_deferredCloudSave > 0)
		{
			const u32 currentTime = fwTimer::GetSystemTimeInMilliseconds();

			if (currentTime-m_save.m_lastcloudSavegame >= m_save.m_deferredCloudSave)
			{
				//Clear cash transactions blocking.
				if (rlTelemetry::GetDeferredFlushBlocked())
				{
					statWarningf("UnBlock Immediate Flush - Deferred Cloud Save being done.");
					rlTelemetry::SetDeferredFlushBlocked( false );
				}

				statDebugf1("Cloud storage: SAVE Performing Deferred Cloud Save - Time since last flush %d.", currentTime - m_save.m_lastcloudSavegame);
				m_save.m_CurrentSlot = STAT_MP_CATEGORY_DEFAULT;
				QueueSave(STATS_SAVE_CLOUD);
			}
		}

		CheckForPendingProfileStatsFlush();
	}
}

bool
CStatsSavesMgr::CheckForPendingProfileStatsFlush( )
{
	//We only have a DEFERRED Profile stats flush pending.
	if (m_save.m_deferredProfileStatsFlush > 0 && CanFlushProfileStats())
	{
		const u32 currentTime = fwTimer::GetSystemTimeInMilliseconds();
		if ((currentTime-m_save.m_lastFlushProfileStatTime)+1 >= m_save.m_deferredProfileStatsFlush)
		{
#if __ASSERT
			const u32 currentTime = fwTimer::GetSystemTimeInMilliseconds();
			if (currentTime-m_save.m_lastFlushProfileStatTime <= m_save.m_minForceFlushTime)
			{
				statAssertf(!m_save.m_IsCritical, "Profile stats being flushed in the again, time since last flush=%d, min flush is %d seconds", currentTime-m_save.m_lastFlushProfileStatTime, m_save.m_minForceFlushTime/1000);
			}
#endif

			statDebugf1("Cloud storage: SAVE Performing Deferred Profile Stats FLUSH - Time since last flush %d.", currentTime - m_save.m_lastFlushProfileStatTime);

			//Clear cash transactions blocking.
			if (rlTelemetry::GetDeferredFlushBlocked( ))
			{
				statWarningf("UnBlock Immediate Flush - Deferred Profile Stats Flush being done.");
				rlTelemetry::SetDeferredFlushBlocked( false );
			}

			TriggerProfileStatsFlush(m_save.m_RequestedSaveTelemetry);

			return true;
		}
	}

	return false;
};

void
CStatsSavesMgr::FlushProfileStats( )
{
	bool bIgnoreServerAuthSync = true;
	Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("IGNORE_CASH_SERVER_VALUES", 0xbd11bd22), bIgnoreServerAuthSync);
	if (bIgnoreServerAuthSync && !GetBlockSaveRequests())
	{
		s_FlushProfileStats = true;

		//Make an ambient Save, 5 minutes from now!
		if (m_save.m_deferredProfileStatsFlush == 0)
		{
			CPed* player = CGameWorld::FindLocalPlayer();
			if(player && !player->GetIsInInterior() && CStatsMgr::PlayerCharIsValidAndPlayingFreemode())
			{
				Save(STATS_SAVE_CLOUD, STAT_MP_CATEGORY_DEFAULT, STAT_SAVETYPE_PHOTOS);

				//Wait for a minimun of 60 seconds in case we were going to do it now.
				const u32 currentTime = fwTimer::GetSystemTimeInMilliseconds();
				if (currentTime-m_save.m_lastFlushProfileStatTime >= m_save.m_deferredProfileStatsFlush)
					m_save.m_deferredProfileStatsFlush = ((currentTime-m_save.m_lastFlushProfileStatTime) + 60000);
			}
		}
	}
}

void 
CStatsSavesMgr::SetupSavegameSlot(const bool save, const int slot)
{
	if (slot < STAT_MP_CATEGORY_DEFAULT)
	{
		//Retrieve the current slot from the chosen character slot.
		if (save)
		{
			m_save.m_CurrentSlot = StatsInterface::GetCurrentMultiplayerCharaterSlot();
			++m_save.m_CurrentSlot;
		}
		else
		{
			m_load.m_CurrentSlot = StatsInterface::GetCurrentMultiplayerCharaterSlot();

			//Make sure the character slot 0 is really being used
			if (m_load.m_CurrentSlot != 0 || StatsInterface::GetBooleanStat(STAT_MP0_CHAR_ISACTIVE))
			{
				++m_load.m_CurrentSlot;
			}
		}
	}
	else
	{
		if (save)
		{
			m_save.m_CurrentSlot = slot;
		}
		else
		{
			m_load.m_CurrentSlot = slot;
		}
	}

	if (save)
	{
		if (m_save.m_OverrideSlot > STAT_MP_CATEGORY_DEFAULT)
		{
			m_save.m_CurrentSlot = m_save.m_OverrideSlot;
		}
	}
	else
	{
		if (m_load.m_OverrideSlot > STAT_MP_CATEGORY_DEFAULT)
		{
			m_load.m_CurrentSlot = m_load.m_OverrideSlot;
		}
	}
}

bool
CStatsSavesMgr::QueueCheckCloudFileLoadPending()
{
	CheckCloudFileLoadPending();
	SetupSavegameSlot(false);

	return IsLoadPending(m_load.m_CurrentSlot);
}

bool 
CStatsSavesMgr::IsLoadInProgress(const int slot) const
{
	if (!m_Initialized)
		return false;

	if (m_SynchProfileStats)
		return true;

	bool pending = m_load.m_InProgress;

	//Check pending loading for a certain slot.
	if(pending && STAT_INVALID_SLOT < slot && slot < TOTAL_NUMBER_OF_FILES)
	{
		pending = (slot == (int)m_load.m_CurrentSlot);
	}

	return pending;
}

bool  
CStatsSavesMgr::IsSaveInProgress() const 
{
	if (!m_Initialized)
		return false;

	if (!NetworkInterface::IsCloudAvailable())
		return false;

	return m_save.m_InProgress;
}

bool 
CStatsSavesMgr::Load(const eStatsLoadType type, const int file)
{
	bool result = false;

	// No network
	if (PARAM_nonetwork.Get() || PARAM_lan.Get() || PARAM_nompsavegame.Get() || PARAM_cloudForceAvailable.Get())
	{
		statErrorf("Cloud storage: LOAD - PARAM_nonetwork.Get() || PARAM_lan.Get() || PARAM_nompsavegame.Get() || PARAM_cloudForceAvailable.Get()");
		return result;
	}

	if (!m_Initialized)
	{
		statErrorf("Cloud storage: LOAD - !m_Initialized");
		return result;
	}


	if (!statVerify(!IsLoadInProgress(STAT_INVALID_SLOT)))
	{
		statErrorf("Cloud storage: LOAD - IsLoadInProgress(STAT_INVALID_SLOT)()");
		return result;
	}

	if (!statVerify(!IsSaveInProgress()))
	{
		statErrorf("Cloud storage: LOAD - IsSaveInProgress()");
		return result;
	}

	if (CGenericGameStorage::IsMpStatsLoadAtTopOfSavegameQueue())
	{
		statErrorf("Cloud storage: LOAD - CGenericGameStorage::IsMpStatsLoadAtTopOfSavegameQueue()");
		return result;
	}

	statAssertf(!GetBlockSaveRequests(), "Cloud storage: LOAD - saves are blocked - you can NOT any loads");
	if (GetBlockSaveRequests())
	{
		statErrorf("Cloud storage: LOAD - saves are blocked - you can NOT any loads");
		return result;
	}

	statAssertf(!m_save.HasRequests(file), "Cloud storage: LOAD - there are pending saves for file %d.", file);
	if (m_save.HasRequests(file))
	{
		statErrorf("Cloud storage: LOAD - there are pending saves for file %d.", file);
		return result;
	}

	//Setup the slot Number
	if (file > STAT_MP_CATEGORY_DEFAULT && statVerify(file < TOTAL_NUMBER_OF_FILES))
	{
		m_load.m_CurrentSlot = file;
	}

	if (type == STATS_LOAD_CLOUD && !m_SynchProfileStats && statVerify(NetworkInterface::IsCloudAvailable()))
	{
		result = QueueLoad(type);

		if (!result)
		{
			statErrorf("Cloud storage: LOAD failed - QueueLoad(type);.");
		}
	}

	//Setup the slot Number
	if (result && file > STAT_MP_CATEGORY_DEFAULT && statVerify(file < TOTAL_NUMBER_OF_FILES))
	{
		m_load.m_OverrideSlot = file;
	}

	return result;
}

bool  
CStatsSavesMgr::QueueLoad(const eStatsLoadType type)
{
	bool result = false;

	if (!statVerify(m_Initialized))
	{
		statErrorf("Cloud storage: LOAD - !m_Initialized");
		return result;
	}

	// No network
	if (PARAM_nonetwork.Get() || PARAM_lan.Get() || PARAM_nompsavegame.Get() || PARAM_cloudForceAvailable.Get())
	{
		statErrorf("Cloud storage: LOAD - PARAM_nonetwork.Get() || PARAM_lan.Get() || PARAM_nompsavegame.Get() || PARAM_cloudForceAvailable.Get()");
		return result;
	}

	if (!statVerify(!IsLoadInProgress(STAT_INVALID_SLOT)))
	{
		statErrorf("Cloud storage: LOAD - IsLoadInProgress(STAT_INVALID_SLOT)");
		return result;
	}

	if (!statVerify(!IsSaveInProgress() || m_load.m_CurrentSlot != m_save.m_CurrentSlot))
	{
		statErrorf("Cloud storage: LOAD - !statVerify(!IsSaveInProgress() || m_load.m_CurrentSlot != m_save.m_CurrentSlot)");
		return result;
	}

	if (CGenericGameStorage::IsMpStatsLoadAtTopOfSavegameQueue())
	{
		statErrorf("Cloud storage: LOAD - IsMpStatsLoadAtTopOfSavegameQueue()");
		return result;
	}

	statAssertf(!GetBlockSaveRequests(), "Cloud storage: LOAD - saves are blocked - you can NOT any loads");
	if (GetBlockSaveRequests())
	{
		statErrorf("Cloud storage: LOAD - LOAD - saves are blocked - you can NOT any loads()");
		return result;
	}

	statAssertf(!m_save.HasRequests(m_load.m_CurrentSlot), "Cloud storage: LOAD - there are pending save for file %d.", m_load.m_CurrentSlot);
	if (m_save.HasRequests(m_load.m_CurrentSlot))
	{
		statErrorf("Cloud storage: LOAD - there are pending save for file %d.", m_load.m_CurrentSlot);
		return result;
	}

	if (statVerify(type == STATS_LOAD_CLOUD) && !m_load.m_InProgress && (IsLoadPending(m_load.m_CurrentSlot) || CloudLoadFailed(m_load.m_CurrentSlot)))
	{
		if (!NetworkInterface::IsCloudAvailable())
			return result;

		if (!NetworkInterface::HasValidRockstarId() && StatsInterface::GetCloudSavegameService() == RL_CLOUD_ONLINE_SERVICE_SC)
			return result;

#if __MPCLOUDSAVES_ONLY_ONEFILE
		statAssertf(m_load.m_CurrentSlot == STAT_MP_CATEGORY_DEFAULT, "Cloud storage: LOAD start for category=%d, can't do this whne we only have one file.", m_load.m_CurrentSlot);
#endif // __MPSAVES_ONLY_ONEFILE

		result = m_load.m_InProgress = CGenericGameStorage::QueueMpStatsLoad(m_load.m_CurrentSlot);

		if (!m_load.m_InProgress)
		{
			statErrorf("Cloud storage: LOAD start has failed for category=%d.", m_load.m_CurrentSlot);
		}
		else
		{
			statDebugf1("Cloud storage: LOAD started for category=%d.", m_load.m_CurrentSlot);

			CSavingMessage::BeginDisplaying(CSavingMessage::STORAGE_DEVICE_MESSAGE_CLOUD_LOADING);

			ClearLoadFailed(m_load.m_CurrentSlot);

			SetLoadPending(m_load.m_CurrentSlot);

			if ( m_load.m_CurrentSlot == STAT_MP_CATEGORY_DEFAULT )
			{
				rlCreateUUID(&m_LoadUid);
			}
		}
	}
	else
	{
		statErrorf("Cloud storage: LOAD - if (statVerify(type == STATS_LOAD_CLOUD) && !m_load.m_InProgress && (IsLoadPending(m_load.m_CurrentSlot) || CloudLoadFailed(m_load.m_CurrentSlot))).");
	}

	return result;
}

bool  
CStatsSavesMgr::Save(const eStatsSaveType /*type*/, const u32 file, const eSaveTypes savetype, const u32 uSaveDelay, const int saveReason)
{
	if (!statVerify(m_Initialized))
	{
		NOTFINAL_ONLY(statWarningf("Cloud storage: Not initialized - SAVE requested and FAILED for file slot=%d.", file));
		return false;
	}

	// No network
	if (PARAM_nonetwork.Get() || PARAM_lan.Get() || PARAM_nompsavegame.Get() || PARAM_cloudForceAvailable.Get())
    {
        NOTFINAL_ONLY(statWarningf("Cloud storage: Bad Command Args (nonetwork/lan/nompsavegame/cloudforceavailable) - SAVE requested and FAILED for file slot=%d.", file));
		return false;
    }

	statAssertf(file<TOTAL_NUMBER_OF_FILES, "FAIL SAVE REQUEST: Invalid file number %d", file);
	if (file>=TOTAL_NUMBER_OF_FILES)
	{
		NOTFINAL_ONLY(statWarningf("Cloud storage: Invalid file number - SAVE requested and FAILED for file slot=%d.", file));
		return false;
	}

	statAssertf(!IsLoadPending(file), "FAIL SAVE REQUEST: Load has not been done for file slot < %d >", file);
	if (IsLoadPending(file))
	{
		NOTFINAL_ONLY(statWarningf("Cloud storage: Load Pending - SAVE requested and FAILED for file slot=%d.", file));
		return false;
	}

	statAssertf(!CloudLoadFailed(file), "FAIL SAVE REQUEST: Load Failed for file slot < %d >", file);
	if (CloudLoadFailed(file))
	{
		NOTFINAL_ONLY(statWarningf("Cloud storage: Load Failed - SAVE requested and FAILED for file slot=%d.", file));
		return false;
	}

	statAssertf(!GetBlockSaveRequests(), "FAIL SAVE REQUEST: Save requests are blocked, SAVE requested for file slot=< %d >", file);
	if (GetBlockSaveRequests())
	{
		statWarningf("Cloud storage: SAVES ARE BLOCKED - SAVE requested for file slot=%d.", file);
		return false;
	}

	statAssertf(!DirtyProfileStatsReadDetected(), "FAIL SAVE REQUEST: Dirty read has been detected in profile stats, SAVE requested for file slot=< %d >", file);
	if (DirtyProfileStatsReadDetected())
	{
		statWarningf("Cloud storage: DIRTY READ DETECTED IN PROFILE STATS - SAVE requested for file slot=%d.", file);
		return false;
	}

	statAssertf(!DirtyCloudReadDetected(), "FAIL SAVE REQUEST: Dirty read has been detected in cloud, SAVE requested for file slot=< %d >", file);
	if (DirtyCloudReadDetected())
	{
		statWarningf("Cloud storage: DIRTY READ DETECTED IN cloud - SAVE requested for file slot=%d.", file);
		return false;
	}

	CProfileStats& profileStatsMgr = CLiveManager::GetProfileStatsMgr();
	if (!profileStatsMgr.Synchronized(CProfileStats::PS_SYNC_MP))
	{
		statWarningf("Cloud storage: NEED TO SYNCHRONIZE PROFILE STATS BEFORE - SAVE requested for file slot=%d.", file);

		if (NetworkInterface::IsNetworkOpen())
		{
			int errorCode = 0;
			if (profileStatsMgr.SynchronizedFailed( CProfileStats::PS_SYNC_MP, errorCode ) && !profileStatsMgr.SynchronizePending( CProfileStats::PS_SYNC_MP ) && profileStatsMgr.FlushFailed(  ))
			{
				statWarningf("Cloud storage: NEED TO SYNCHRONIZE PROFILE STATS BEFORE - FORCING A SYNCH - SAVE requested for file slot=%d.", file);
				profileStatsMgr.Synchronize( true, true );
			}
		}

		return false;
	}

#if __MPCLOUDSAVES_ONLY_ONEFILE
	if (file > STAT_MP_CATEGORY_DEFAULT)
	{
		statWarningf("Cloud storage: USING ONLY ONE FILE - IGNORE SAVE requested for file slot=%d.", file);
		return true;
	}
#endif // __MPSAVES_ONLY_ONEFILE

	//No SAVING
	if (!m_saveHistory.CanSave(file, savetype))
	{
		NOTFINAL_ONLY(statWarningf("Cloud storage: SAVE IGNORED for file slot=%d, savetype=%s.", file, GET_STAT_SAVETYPE_NAME(savetype));)
		return true;
	}

	//Perform a profile stats flush when 10m have passed since the last flush.
	if (0 == m_save.m_deferredProfileStatsFlush && m_saveHistory.DeferredProfileStatsFlush(savetype))
	{
		NOTFINAL_ONLY(statDebugf1("Cloud storage: SAVE requested for file slot=%d, savetype=%s - Deferred ProfileStats Flush.", file, GET_STAT_SAVETYPE_NAME(savetype));)
		const u32 PROFILE_STATS_FLUSH_INTERVAL = 10*60*1000;
		m_save.m_deferredProfileStatsFlush = PROFILE_STATS_FLUSH_INTERVAL;
	}

	//Perform a Cloud save when 10m have passed since the last save.
	if (0 == m_save.m_deferredCloudSave && m_saveHistory.DeferredCloudSave(savetype) && file == STAT_MP_CATEGORY_DEFAULT)
	{
		NOTFINAL_ONLY(statDebugf1("Cloud storage: SAVE requested for file slot=%d, savetype=%s - Deferred Cloud Save.", file, GET_STAT_SAVETYPE_NAME(savetype));)
		const u32 PROFILE_STATS_CLOUDSAVE_INTERVAL = 10*60*1000;
		m_save.m_deferredCloudSave = PROFILE_STATS_CLOUDSAVE_INTERVAL;
	}

	//We want a profile stats flush NOW.
	if (m_saveHistory.RequestProfileStatsFlush(savetype) && statVerify(!m_saveHistory.CloudSaveOnly(savetype)))
	{
		NOTFINAL_ONLY(statDebugf1("Cloud storage: SAVE requested for file slot=%d, savetype=%s - Requested a profile stats FLUSH.", file, GET_STAT_SAVETYPE_NAME(savetype));)
		m_save.m_flushProfileStatOnNextCloudSave = false;
		m_save.m_flushProfileStat = true;
		m_saveHistory.SaveRequested(savetype);
		
		m_save.m_RequestedSaveTelemetry.SetFrom(savetype, saveReason);

		//Clear cash transactions blocking.
		if (rlTelemetry::GetDeferredFlushBlocked( ))
		{
			NOTFINAL_ONLY( statWarningf("UnBlock Immediate Flush - save called slot=%d, savetype=%s.", file, GET_STAT_SAVETYPE_NAME(savetype)); )
			rlTelemetry::SetDeferredFlushBlocked( false );
		}
	}

	//We want a cloud save NOW.
	if (m_saveHistory.RequestCloudSave(savetype) && statVerify(!m_saveHistory.ProfileStatsFlushOnly(savetype)))
	{
		//Clear cash transactions blocking.
		if (rlTelemetry::GetDeferredFlushBlocked( ))
		{
			NOTFINAL_ONLY( statWarningf("UnBlock Immediate Flush - save called slot=%d, savetype=%s.", file, GET_STAT_SAVETYPE_NAME(savetype)); )
			rlTelemetry::SetDeferredFlushBlocked( false );
		}

		m_saveHistory.SaveRequested(savetype);

		m_save.m_RequestSlot |= (1<<file);

		m_save.m_RequestedSaveTelemetry.SetFrom(savetype, saveReason);

		//Flush Profile stats on next cloud Save requested.
		if (file == STAT_MP_CATEGORY_DEFAULT && m_save.m_flushProfileStatOnNextCloudSave)
		{
			m_save.m_flushProfileStatOnNextCloudSave = false;
			m_save.m_flushProfileStat = true;
		}

		if (savetype >= STAT_SAVETYPE_CRITICAL)
		{
			m_save.m_IsCritical |= (1<<file);
		}

		const u32 currTime = sysTimer::GetSystemMsTime();
		if (m_save.m_RetryFailedSave > 0)
		{
			NOTFINAL_ONLY(statDebugf1("Cloud storage: SAVE requested for file slot=%d, savetype=%s, will be started in time=%ds.", file, GET_STAT_SAVETYPE_NAME(savetype), m_save.m_NextSaveTime - currTime));
		}
		else
		{
			NOTFINAL_ONLY(statDebugf1("Cloud storage: SAVE requested for file slot=%d, savetype=%s, will be started in time=%ds.", file, GET_STAT_SAVETYPE_NAME(savetype), uSaveDelay/1000));
			m_save.m_NextSaveTime = currTime + uSaveDelay;
		}
	}
	else if (m_save.m_flushProfileStat)
	{
		NOTFINAL_ONLY(statDebugf1("Cloud storage: SAVE Profile Stats flush requested for file slot=%d, savetype=%s,.", file, GET_STAT_SAVETYPE_NAME(savetype)));

		//m_deferredProfileStatsFlush can not be 0, because 0 means disabled.
		m_save.m_deferredProfileStatsFlush = 1 + uSaveDelay;

		m_save.m_flushProfileStatOnNextCloudSave = false;

		bool result = CheckForPendingProfileStatsFlush( );

		if (uSaveDelay == 0)
			return result;
	}

	return true;
}

bool  
CStatsSavesMgr::QueueSave(const eStatsSaveType type, const u32 file)
{
	bool result = false;

	if (!m_Initialized)
		return result;

	// No network
	if (PARAM_nonetwork.Get() || PARAM_lan.Get() || PARAM_nompsavegame.Get() || PARAM_cloudForceAvailable.Get())
		return result;

	if (!statVerify(!IsLoadInProgress(file)))
		return result;

	if (!statVerify(!IsSaveInProgress()))
		return result;

	if (CGenericGameStorage::IsMpStatsSaveAtTopOfSavegameQueue())
		return result;

	if (GetBlockSaveRequests())
		return result;

	if (DirtyProfileStatsReadDetected())
		return result;

	if (DirtyCloudReadDetected())
		return result;

	//Cloud Storage
	//Save to Cloud if we don't have pending load and if it did not fail to load.
	//  - If the cloud load failed we check if the local one has succeeded and save anyway.
	if (statVerify(type == STATS_SAVE_CLOUD) && !m_save.m_InProgress && statVerifyf(!IsLoadPending(m_save.m_CurrentSlot), "slot %d", m_save.m_CurrentSlot) && statVerifyf(!CloudLoadFailed(m_save.m_CurrentSlot), "slot %d", m_save.m_CurrentSlot))
	{
		if (!NetworkInterface::IsCloudAvailable())
			return result;

		if (!NetworkInterface::HasValidRockstarId() && StatsInterface::GetCloudSavegameService() == RL_CLOUD_ONLINE_SERVICE_SC)
			return result;

		//Check timestamp for Multi Player
		const char* statName = "_SaveMpTimestamp_0";
		switch (m_save.m_CurrentSlot)
		{
		case STAT_MP_CATEGORY_DEFAULT: statName = "_SaveMpTimestamp_0"; break;
		case STAT_MP_CATEGORY_CHAR0:   statName = "_SaveMpTimestamp_1"; break;
		case STAT_MP_CATEGORY_CHAR1:   statName = "_SaveMpTimestamp_2"; break;
		}
		StatId statTimestamp(statName);

		m_save.m_CurrentSlotTimestamp = rlGetPosixTime();

		//Setup save timestamp
		StatsInterface::SetStatData(statTimestamp, m_save.m_CurrentSlotTimestamp, STATUPDATEFLAG_DIRTY_PROFILE);

		static bool s_TryCachingCloudSavegameLocally = false;
		if (m_save.m_CurrentSlot == STAT_MP_CATEGORY_DEFAULT)
		{
			s_TryCachingCloudSavegameLocally = false;
			m_save.m_deferredCloudSave = 0;
			m_save.m_InProgressSaveTelemetry = m_save.m_RequestedSaveTelemetry;
		}

		if (m_save.m_lastcloudSavegame > 0 && m_save.m_CurrentSlot == STAT_MP_CATEGORY_DEFAULT && !m_save.IsCritical(STAT_MP_CATEGORY_DEFAULT))
		{
			int minSavegameInterval = eSaveOperations::MIN_CLOUD_SAVE_INTERVAL;
			Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("MIN_CLOUD_SAVE_INTERVAL", 0x1b6a88a4), minSavegameInterval);

			const u32 currTime = fwTimer::GetSystemTimeInMilliseconds();
			if (currTime-m_save.m_lastcloudSavegame < minSavegameInterval)
			{
				statDebugf1("Cloud storage: SAVE request for file %d, has been set to be cached locally.", m_save.m_CurrentSlot);
				s_TryCachingCloudSavegameLocally = true;
			}
		}

#if __ALLOW_LOCAL_MP_STATS_SAVES
		CSavegameQueuedOperation_MPStats_Save::eMPStatsSaveDestination saveDestination = CSavegameQueuedOperation_MPStats_Save::MP_STATS_SAVE_TO_CLOUD;

		//We want to cache the savegame locally on the console.
		if (s_TryCachingCloudSavegameLocally)
		{
#if !__FINAL
			if (PARAM_enableLocalMPSaveCache.Get())
#endif
			{
				saveDestination = CSavegameQueuedOperation_MPStats_Save::MP_STATS_SAVE_TO_CONSOLE;
			}
		}
#endif	//	__ALLOW_LOCAL_MP_STATS_SAVES

		result = m_save.m_InProgress = CGenericGameStorage::QueueMpStatsSave(m_save.m_CurrentSlot
#if __ALLOW_LOCAL_MP_STATS_SAVES
			, saveDestination
#endif
			);

		m_save.m_CurrentSlotIsCritical = m_save.IsCritical(m_save.m_CurrentSlot);
		m_save.ClearRequest(m_save.m_CurrentSlot);

#if __MPCLOUDSAVES_ONLY_ONEFILE
		statAssertf(m_save.m_CurrentSlot == STAT_MP_CATEGORY_DEFAULT, "Cloud storage: SAVE started for category=%d, cant do this when we only have one file.", m_save.m_CurrentSlot);
#endif // __MPSAVES_ONLY_ONEFILE

		if (m_save.m_InProgress)
		{
			statDebugf1("Cloud storage: SAVE started for category=%d.", m_save.m_CurrentSlot);
			CSavingMessage::BeginDisplaying(CSavingMessage::STORAGE_DEVICE_MESSAGE_CLOUD_SAVING);

			if (!s_TryCachingCloudSavegameLocally)
			{
				m_save.m_lastcloudSavegame = fwTimer::GetSystemTimeInMilliseconds();
			}
		}
		else
		{
			statErrorf("Cloud storage: SAVE started has failed for category=%d.", m_save.m_CurrentSlot);
			s_TryCachingCloudSavegameLocally = false;
		}
	}

	//Setup the slot Number
	if (result && file > STAT_MP_CATEGORY_DEFAULT && statVerify(file < TOTAL_NUMBER_OF_FILES))
	{
		m_save.m_OverrideSlot = file;
	}

	return result;
}

void  
CStatsSavesMgr::ClearLoadPending(const int file)
{
	//Set stats as synched so they can be synched with profile stats server.
	StatsInterface::SetAllStatsToSynched(file, true);

	m_load.m_PendingSlot &= (~(1<<file));
}

void  
CStatsSavesMgr::CheckCloudFileLoadPending()
{
	if (!StatsInterface::GetBooleanStat(STAT_MP0_CHAR_ISACTIVE))
	{
		statDebugf1("Cloud - Character Slot 1 not being used remove need to load savegame files.");
		ClearLoadPending(STAT_MP_CATEGORY_CHAR0);
	}

	if (!StatsInterface::GetBooleanStat(STAT_MP1_CHAR_ISACTIVE))
	{
		statDebugf1("Cloud - Character Slot 2 not being used remove need to load savegame files.");
		ClearLoadPending(STAT_MP_CATEGORY_CHAR1);
	}
}

bool
CStatsSavesMgr::GetCharacterActive(const u32 character)
{
	char statName[20];
	sysMemSet(statName, 0, 20);
	sprintf(statName, "MP%1d_CHAR_ISACTIVE", character);

	return StatsInterface::GetBooleanStat(StatId(statName));
}

bool  
CStatsSavesMgr::PendingSaveRequests( ) const
{
	bool isPending = false;

	NOTFINAL_ONLY( int slot = -1; )

	//We can Only save if there are no pending load requests.
	for (int i=0; i<TOTAL_NUMBER_OF_FILES && !isPending; i++)
	{
		if (m_save.HasRequests(i) && !IsLoadPending(i))
		{
			NOTFINAL_ONLY( slot = i; )
			isPending = true;
			break;
		}
	}

#if !__FINAL
	static bool s_isPending = false;

	if (isPending && !s_isPending)
		statDebugf1("PendingSaveRequests: slot='%d', pending='%s'", slot, isPending?"true":"false");
	else if (!isPending && s_isPending)
		statDebugf1("PendingSaveRequests: slot='%d', pending='%s'", slot, isPending?"true":"false");

	s_isPending = isPending;
#endif // !__FINAL

	return isPending;
}

void 
CStatsSavesMgr::SetNoRetryOnFail(bool value)
{
	m_NoRetryOnFail = value;
}

bool
CStatsSavesMgr::CanFlushProfileStats( ) const
{
	if (IsLoadPending( STAT_MP_CATEGORY_DEFAULT ))
		return false;
	if (CloudSaveFailed( STAT_MP_CATEGORY_DEFAULT ))
		return false;

	const int selectedSlot = StatsInterface::GetCurrentMultiplayerCharaterSlot() + 1;
	if (IsLoadPending( selectedSlot ))
		return false;

	if (CloudSaveFailed( selectedSlot ))
		return false;

	if (IsSaveInProgress( ))
		return false;

	if (PendingSaveRequests( ))
		return false;

	statAssertf(!GetBlockSaveRequests(), "CanFlushProfileStats: Save requests are blocked.");
	if (GetBlockSaveRequests())
		return false;

	statAssertf(!DirtyProfileStatsReadDetected(), "CanFlushProfileStats: Dirty read has been detected in profile stats.");
	if (DirtyProfileStatsReadDetected())
		return false;

	statAssertf(!DirtyCloudReadDetected(), "CanFlushProfileStats: Dirty read has been detected in cloud.");
	if (DirtyCloudReadDetected())
		return false;

	return true;
}

void
CStatsSavesMgr::CancelAndClearSaveRequests( )
{
	statDebugf1("CancelAndClearSaveRequests: Canceling save requests.");

	ClearPendingFlushRequests();

	for (int i = 0; i <= STAT_MP_CATEGORY_MAX; i++)
		ClearPendingSaveRequests(i);

	CLiveManager::GetProfileStatsMgr().CancelPendingFlush();
}

int
CStatsSavesMgr::GetNextPendingSaveRequest() const
{
	for (int i=0; i<TOTAL_NUMBER_OF_FILES; i++)
	{
		if (m_save.HasRequests(i) && !IsLoadPending(i))
		{
			return i;
		}
	}

	return STAT_INVALID_SLOT;
}

bool
CStatsSavesMgr::RetrySaveOnHttpErrorCode(const int resultcode) const
{
	//Dont retry saving if we are not in Multiplayer. Saves might have failed and we want to stay like that.
	if (!NetworkInterface::IsNetworkOpen())
		return false;

	if(m_NoRetryOnFail)
		return false;

	if (resultcode == HTTP_CODE_REQUESTTIMEOUT)
		return true;
	else if (resultcode == HTTP_CODE_TOOMANYREQUESTS)
		return true;
	else if (resultcode == HTTP_CODE_GATEWAYTIMEOUT)
		return true;
	//All 5xx error codes
	else if (resultcode >= HTTP_CODE_ANYSERVERERROR && resultcode <= HTTP_CODE_NETCONNECTTIMEOUT)
		return true;

	return false;
}

void
CStatsSavesMgr::TriggerProfileStatsFlush(CSaveTelemetryStats saveTelemetry)
{
	if (!GetBlockSaveRequests())
	{
		CSavingMessage::BeginDisplaying(CSavingMessage::STORAGE_DEVICE_MESSAGE_CLOUD_SAVING);
		CLiveManager::GetProfileStatsMgr().Flush( true, true );

		//Send save game telemetry.
		saveTelemetry.FlushTelemetry(false, false);
	}

	s_FlushProfileStats = false;

	m_save.m_lastFlushProfileStatTime  = fwTimer::GetSystemTimeInMilliseconds();
	m_save.m_flushProfileStat          = false;
	m_save.m_deferredProfileStatsFlush = 0;
	m_save.m_flushProfileStatOnNextCloudSave = false;

	ASSERT_ONLY(m_save.m_lastFlushProfileStatWasDeferred = false);
#if !__NO_OUTPUT
	if (PARAM_spewDirtySavegameStats.Get())
		CStatsMgr::GetStatsDataMgr().SpewDirtyCloudOnlySats( true );
#endif // !__NO_OUTPUT
}

void
CStatsSavesMgr::SaveLocalCloudDirtyReadTimestamp()
{
	CProfileSettings& settings = CProfileSettings::GetInstance();
	statAssert( settings.AreSettingsValid() );

	CProfileSettings::ProfileSettingId idLow32 = CProfileSettings::MP_CLOUD_POSIXTIME_LOW32;
	CProfileSettings::ProfileSettingId idHigh32 = CProfileSettings::MP_CLOUD_POSIXTIME_HIGH32;

	switch(m_save.m_CurrentSlot)
	{
	case STAT_MP_CATEGORY_CHAR0: idLow32 = CProfileSettings::MP_CLOUD0_POSIXTIME_LOW32; idHigh32 = CProfileSettings::MP_CLOUD0_POSIXTIME_HIGH32; break;
	case STAT_MP_CATEGORY_CHAR1: idLow32 = CProfileSettings::MP_CLOUD1_POSIXTIME_LOW32; idHigh32 = CProfileSettings::MP_CLOUD1_POSIXTIME_HIGH32; break;

	case STAT_MP_CATEGORY_DEFAULT:
		break;
	}

	settings.Set(idLow32,  (int)(m_save.m_CurrentSlotTimestamp & 0xFFFFFFFF));
	settings.Set(idHigh32, (int)(m_save.m_CurrentSlotTimestamp >> 32));

	settings.Write( true );
}

// EOF

