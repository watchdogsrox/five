//
// NetworkDebugTelemetry.cpp
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#if RSG_OUTPUT

#include "atl/pool.h"
#include "crclip/clipplayer.h"
#include "net/nethardware.h"
#include "system/simpleallocator.h"
#include "system/param.h"
#include "grcore/setup.h"
#include "grcore/grcorespu.h"
#include "grmodel/setup.h"
#include "grprofile/timebars.h"

#include "fwnet/netchannel.h"
#include "fwnet/netutils.h"
#include "net/netfuncprofiler.h"
#include "fwdrawlist/drawlistmgr.h"
#include "entity/archetypemanager.h"
#include "NetworkDebugTelemetry.h"
#include "Network/Live/NetworkTelemetry.h"
#include "optimisations.h"

#include "scene/animatedbuilding.h"
#include "scene/scene.h"
#include "scene/LoadScene.h"
#include "scene/streamer/SceneStreamerMgr.h"
#include "streaming/streamingdebug.h"
#include "tools/SmokeTest.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "Peds/ped.h"
#include "game/Clock.h"
#include "cutscene/CutSceneManagerNew.h"

#include "renderer/PostScan.h"

#include "stats/StatsInterface.h"
#include "script/script.h"

#if _METRICS_ENABLE_CLIPS
#include <algorithm>
#include "atl/array.h"
#include "system/criticalsection.h"
#endif

#if ENABLE_DEBUG_HEAP
#include "system/memory.h"
#endif

NETWORK_OPTIMISATIONS()

XPARAM(nonetlogs);
XPARAM(nonetwork);
PARAM(telemetrycoordsinterval, "Set the interval we send the coordinates packet.");
PARAM(telemetrycoordsdistance, "Set the distance to previous coords we send the coordinates packet.");

#if _METRICS_ENABLE_CLIPS
PARAM(telemetryclipsplayed, "Activate clips played metrics.");
#endif // _METRICS_ENABLE_CLIPS

PARAM(addTelemetryDebugTTY, "Output some data to the tty about where test metric data is sent from.");

PARAM(telemetrynodebug, "Do not allow any debug metrics.");


RAGE_DEFINE_SUBCHANNEL(net, debug_telemetry, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_debug_telemetry

#undef __tel_loglevel
#define __tel_loglevel LOGLEVEL_DEBUG1

#undef __tel_logchannel
#define __tel_logchannel TELEMETRY_CHANNEL_DEV_DEBUG

//Metrics in this channel are off by default unless the param 
#undef __tel_logleveldevoff
#define __tel_logleveldevoff LOGLEVEL_DEBUG_NEVER

static const unsigned DEFAULT_COORDS_INTERVAL = 10000;        //interval between sending player coords
static const unsigned MINIMUM_COORDS_INTERVAL = 10000;
static const float DEFAULT_COORDS_DELTA       = 5;            //Delta required before coords are written
static const float MINIMUM_COORDS_DELTA       = 5;
unsigned    CNetworkDebugTelemetry::m_CoordsInterval = DEFAULT_COORDS_INTERVAL;
float       CNetworkDebugTelemetry::m_CoordsDelta    = DEFAULT_COORDS_DELTA;
unsigned    CNetworkDebugTelemetry::m_LastCoordsTime;

#if RAGE_POOL_TRACKING
static const unsigned POOLUSAGE_INTERVAL = 10*60*1000;    //interval between sending memory stats
static bool s_SendMemoryPoolMetric = false;
unsigned    CNetworkDebugTelemetry::m_LastPoolMemDebugTime = 0;
#endif

#if ENABLE_STORETRACKING
static const unsigned DRAWLIST_INTERVAL = 5000; //interval between sending Draw List stats
static bool s_SendDrawListMetric = false;
unsigned    CNetworkDebugTelemetry::m_LastDrawListTime = 0;

static const unsigned MEMUSAGE_INTERVAL = 10*60*1000; //interval between sending memory usage stats
static const unsigned MEMSTORE_INTERVAL = 10*60*1000; //interval between sending memory store debug stats
static bool s_SendMemoryStoreMetric = false;
static bool s_SendMemoryUsageMetric = false;
unsigned    CNetworkDebugTelemetry::m_LastMemUsageTime = 0;
unsigned    CNetworkDebugTelemetry::m_LastStoreMemDebugTime = 0;
#endif // ENABLE_STORETRACKING

#if ENABLE_DEBUG_HEAP
static const unsigned MEMCONSOLE_INTERVAL = 10 * 60000;   //interval between sending console memory debug size
unsigned    CNetworkDebugTelemetry::m_LastConsoleMemDebugTime = 0;
#endif // ENABLE_DEBUG_HEAP

#if _METRICS_ENABLE_CLIPS
static  const unsigned  MAX_NUMBER_CLIPS     = 300;     //Maximum number of different clips we can gather info about.
static  const unsigned  CLIPSPLAYED_INTERVAL = 15*60*1000; //interval between sending clips played metrics.
static  const unsigned  CLIPSPLAYED_MISSIONENDED_INTERVAL = 20*1000; //interval between sending clips played metrics.
static  unsigned  s_SendClipsPlayedMetric = 0;

//Played clip data info
struct sMetricClipInfo
{
public:
	u32   m_KeyName;
	u32   m_ClipName;
	u32   m_DictionaryName;
	u32   m_TimesPlayed;
	sMetricClipInfo() : m_KeyName(0), m_ClipName(0), m_DictionaryName(0), m_TimesPlayed(1) {}

	bool operator<(const sMetricClipInfo& other) const { return (m_KeyName < other.m_KeyName); }
	bool operator<(const u32& key) const { return (m_KeyName < key); }
	bool operator==(const sMetricClipInfo& other) const { return (m_KeyName == other.m_KeyName); }
	bool operator==(const u32& key) const { return (m_KeyName == key); }

	const sMetricClipInfo& operator=(const sMetricClipInfo& other)
	{
		if(&other != this)
		{
			m_KeyName        = other.m_KeyName;
			m_ClipName       = other.m_ClipName;
			m_DictionaryName = other.m_DictionaryName;
			m_TimesPlayed    = other.m_TimesPlayed;
		}
		return *this;
	}
};
static atFixedArray< sMetricClipInfo, MAX_NUMBER_CLIPS >  s_aClipsPlayedData;
static sysCriticalSectionToken sm_ClipsPlayedCriticalSection;
#endif // _METRICS_ENABLE_CLIPS

static const unsigned TIMEBAR_MAIN   = 1;
static const unsigned TIMEBAR_RENDER = 2;
static const unsigned TIMEBAR_GPU    = 3;

//Add precison Time in microseconds accuracy:
//  Number of seconds since s_MetricsDebugStartTime was set.
static u64       s_MetricsDebugStartTime      = 0;
static utimer_t  s_MetricsDebugStartTickCount = 0;

rlMetricList CNetworkDebugTelemetry::m_DebugMetricList;
unsigned CNetworkDebugTelemetry::m_DebugMetricListSize = 0;
bool CNetworkDebugTelemetry::m_bAllowMetricWithNoCreds = true;
bool CNetworkDebugTelemetry::m_SendTimeBars = false;

struct sMetricInfoTimebar
{
	unsigned  m_Name;
	float     m_Time;
};

static atArray < sMetricInfoTimebar >  s_TimebarMain;
static atArray < sMetricInfoTimebar >  s_TimebarRender;
static atArray < sMetricInfoTimebar >  s_TimebarGpu;
static u64                             s_TimebarTimestamp = 0;
static int                             s_TimebarPosX = 0;
static int                             s_TimebarPosY = 0;

//PURPOSE
//  Append metric Ip address and Mac Address.
bool  AppendMetric_IP_MAC_ADDRESS(RsonWriter* rw)
{
	if (gnetVerify(rw))
	{
		netIpAddress ip;

		netHardware::GetPublicIpAddress(&ip);
		char ipPubStr[netIpAddress::MAX_STRING_BUF_SIZE];
		ip.Format(ipPubStr);

		netHardware::GetLocalIpAddress(&ip);
		char ipLocalStr[netIpAddress::MAX_STRING_BUF_SIZE];
		ip.Format(ipLocalStr);

		u8 macAddr[6];
		netHardware::GetMacAddress(macAddr);

		u64 mac = 0;
		for(int i = 0; i < 6; ++i)
		{
			mac = (mac << 8) | macAddr[i];
		}

		return rw->WriteString("ip_pub", ipPubStr) 
			&& rw->WriteString("ip_local", ipLocalStr) 
			&& rw->WriteUns64("mac", mac);
	}

	return false;
};

//PURPOSE
//  Append metric built type to the metric writer.
bool AppendMetric_BUILD_TYPE(RsonWriter* rw)
{
	if (gnetVerify(rw))
	{
		char build[MetricPlayStat::MAX_STRING_LENGTH];
		formatf(build, "%s", "Other");

#if (__DEV && __OPTIMIZED && __BANK && __ASSERT && RSG_OUTPUT & !__EDITOR && !__TOOL && !__EXPORTER && !__PROFILE)
		formatf(build, "%s", "Beta");
#elif (!__DEV && __OPTIMIZED && __BANK && !__ASSERT && RSG_OUTPUT & !__EDITOR && !__TOOL && !__EXPORTER && !__PROFILE)
		formatf(build, "%s", "BankRelease");
#elif (!__DEV && __OPTIMIZED && !__BANK && !__ASSERT && RSG_OUTPUT & !__EDITOR && !__TOOL && !__EXPORTER && !__PROFILE)
		formatf(build, "%s", "Release");
#elif (__DEV && !__OPTIMIZED && __BANK && __ASSERT && RSG_OUTPUT & !__EDITOR && !__TOOL && !__EXPORTER && !__PROFILE)
		formatf(build, "%s", "Debug");
#endif 

		return rw->WriteUns("build", atStringHash( build));
	}

	return false;
}

class MetricGAMEPLAY_FPS : public MetricPlayerCoords
{
	RL_DECLARE_METRIC(GAMEPLAY_FPS, __tel_logchannel, __tel_loglevel);

private:
	float m_FPS;

public:

	explicit MetricGAMEPLAY_FPS ()
	{
		m_FPS = (1.0f / rage::Max(0.001f, fwTimer::GetSystemTimeStep()));
	}

	virtual bool Write(RsonWriter* rw) const
	{
		return this->MetricPlayerCoords::Write(rw) 
			&& rw->WriteFloat("fps", m_FPS);
	}
};

class MetricBUILD_TYPE : public MetricGAMEPLAY_FPS
{
	RL_DECLARE_METRIC(BUILD_TYPE, TELEMETRY_CHANNEL_DEV_CAPTURE, __tel_loglevel);

public:

	explicit MetricBUILD_TYPE ()
	{
	}

	virtual bool Write(RsonWriter* rw) const
	{
		return this->MetricGAMEPLAY_FPS::Write(rw) 
			&& AppendMetric_BUILD_TYPE(rw);
	}
};


unsigned const VALID_DLS [] = {
	// Essential
	atStringHash( "DL_LIGHTING" )
	,atStringHash( "DL_DRAWSCENE" )
	,atStringHash( "DL_WATER_REFLECTION" )
	,atStringHash( "DL_REFLECTION_MAP" )
	,atStringHash( "DL_SHADOWS" )
	,atStringHash( "DL_CASCADE_SHADOWS" )
	,atStringHash( "DL_GBUF" )
	,atStringHash( "DL_RAIN_UPDATE" )
	,atStringHash( "DL_MIRROR_REFLECTION" )
	,atStringHash( "DL_SCRIPT" )
	,atStringHash( "DL_HUD" )
	,atStringHash( "DL_FRONTEND" )
	,atStringHash( "DL_HEIGHT_MAP" )
	,atStringHash( "DL_PED_DAMAGE_GEN" )

};
static const unsigned SIZE_OF_DLS = sizeof(VALID_DLS) / sizeof(unsigned);

bool  GetIsDrawListValid(const u32 nameHash)
{
	bool result = false;

	for (int i=0; i<SIZE_OF_DLS && !result; i++)
	{
		if (nameHash == VALID_DLS[i])
		{
			result = true;
		}
	}

	return result;
}

#if !RSG_FINAL
class MetricDRAW_LISTS : public MetricBUILD_TYPE
{
	RL_DECLARE_METRIC(DRAW_LISTS, TELEMETRY_CHANNEL_DEV_CAPTURE, __tel_logleveldevoff);

private:
	atFixedArray <u32, SIZE_OF_DLS> m_dln;
	atFixedArray <u32, SIZE_OF_DLS> m_dlc;
	u32           m_Count;
	int           m_idx;
	int           m_gidx;
	bool          m_gp;
	bool          m_ls;
	int           m_au;
	float         m_ut;
	float         m_dt;
	float         m_gt;
	float         m_time; // Metric Time in microseconds accuracy

	u32			  m_ObjectCount;
	u32			  m_DrawcallCount;

	u32			  m_cutsceneNameHash;
	int			  m_cutsceneFrame;
	int			  m_cutsceneTotalFrames;
	int			  m_cutsceneCurrentConcatSection;

public:

	explicit MetricDRAW_LISTS ()
	{
		//Add precision Time in microseconds accuracy:
		//  Number of seconds since s_MetricsDebugStartTime was set.
		m_time = 0.0f;
		if (!PARAM_nonetlogs.Get())
		{
			m_time = (float)((sysTimer::GetTicksToMicroseconds() * (sysTimer::GetTicks() - s_MetricsDebugStartTickCount)) * 1e-6) + (rlGetPosixTime() - s_MetricsDebugStartTime);
		}

		m_dln.clear();
		m_dln.Reset();
		m_dln.Resize(SIZE_OF_DLS);

		m_dlc.clear();
		m_dlc.Reset();
		m_dlc.Resize(SIZE_OF_DLS);

		for (int i=0; i<SIZE_OF_DLS; i++)
		{
			m_dln[i] = 0;
			m_dlc[i] = 0;
		}

		m_Count = 0;

		const u32 invalid = ATSTRINGHASH("INVALID", 0x8451F94F);

		for(int i=0; i<gDrawListMgr->GetDrawListTypeCount(); i++)
		{
			if (0 < gDrawListMgr->GetDrawListEntityCount(i))
			{
				const char* dlName = gDrawListMgr->GetDrawListName(i);

				if (gnetVerify(dlName))
				{
					const u32 nameHash = atStringHash( dlName );

					if (GetIsDrawListValid(nameHash))
					{
						m_dln[m_Count] = nameHash;
						m_dlc[m_Count++] = gDrawListMgr->GetDrawListEntityCount(i);
					}
					else if(invalid != nameHash)
					{
						gnetDebug3("Metric DRAW_LISTS, ignore invalid drawlist with name %s", dlName);
					}
				}
			}
		}

		m_idx  = gDrawListMgr->GetTotalIndices();
		m_gidx = gDrawListMgr->GetGBufIndices();
		m_gp   = fwTimer::IsGamePaused();
		m_ls   = g_LoadScene.IsActive() || g_SceneStreamerMgr.IsLoading();
		m_au   = -1;
		m_ut   = 0.0f;
		m_dt   = 0.0f;
		m_gt   = 0.0f;

		if(SmokeTests::g_automatedSmokeTest > -1)
		{
			m_au = SmokeTests::g_automatedSmokeTest;
		}

		rage::grmSetup* pSetup = CSystem::GetRageSetup();
		if (pSetup)
		{
			m_ut = pSetup->GetUpdateTime();
			m_dt = pSetup->GetDrawTime();
			m_gt = pSetup->GetGpuTime();
		}

		m_ObjectCount = gPostScan.GetEntityCount();

		// Draw Call Counts
		m_DrawcallCount = 0;
		const int maxPhase = gDrawListMgr->GetDrawListTypeCount() - 1;
		for(int i=0;i<maxPhase;i++)
		{
#if DRAWABLESPU_STATS
			const spuDrawableStats&	stats = gDrawListMgr->GetDrawableStats(i);
			for(int j=DCC_NO_CATEGORY;j<DCC_MAX_CONTEXT;j++)
			{
				int stat = stats.DrawCallsPerContext[j];
				m_DrawcallCount += stat;
			}
#elif DRAWABLE_STATS
			const drawableStats& stats = gDrawListMgr->GetDrawableStats(i);
			m_DrawcallCount += stats.TotalDrawCalls;
#endif
		}

		m_cutsceneFrame = -1;
		m_cutsceneTotalFrames = -1;
		CutSceneManager* cutmgr = CutSceneManager::GetInstance();
		if(cutmgr->IsCutscenePlayingBack() && cutmgr->GetCutsceneName())
		{
			m_cutsceneNameHash = atStringHash(cutmgr->GetCutsceneName());
			m_cutsceneFrame = CutSceneManager::GetInstance()->GetCutSceneCurrentFrame();
			m_cutsceneTotalFrames = (u32)(cutmgr->GetTotalSeconds() * CUTSCENE_FPS);
			m_cutsceneCurrentConcatSection = cutmgr->GetConcatSectionForTime( cutmgr->GetCutSceneCurrentTime() );
		}
	}

	~MetricDRAW_LISTS()
	{
		m_dln.clear();
		m_dln.Reset();

		m_dlc.clear();
		m_dlc.Reset();
	}

	virtual bool Write(RsonWriter* rw) const
	{
		bool result = this->MetricBUILD_TYPE::Write(rw);

		if(0.0f < m_time)
		{
			result = result && rw->WriteFloat("tm", m_time, "%.5f");
		}

		if (m_Count > 0)
		{
			result = result && rw->BeginArray("dls", NULL);
			{
				for(int i=0; i < m_Count && result; i++)
				{
					result = result && rw->Begin(NULL,NULL);
						result = result && rw->WriteValue("dln", m_dln[i]);
						result = result && rw->WriteValue("dlc", m_dlc[i]);
					result = result && rw->End();
				}
			}
			result = result && rw->End();
		}

		result = result && rw->WriteInt("idx", m_idx);

		result = result && rw->WriteInt("gidx", m_gidx);

		if(m_gp)
		{
			result = result && rw->WriteBool("gp", true);
		}

		if(m_ls) 
		{ 
			result = result && rw->WriteBool("ls", true); 
		}

		if(m_au > -1)
		{
			result = result && rw->WriteInt("au", m_au);
		}

		if (m_ut > 0.0f || m_dt > 0.0f || m_gt > 0.0f)
		{
			result = result && rw->BeginArray("ut", NULL);
			{
				result = result && rw->WriteValue(NULL, m_ut);
				result = result && rw->WriteValue(NULL, m_dt);
				result = result && rw->WriteValue(NULL, m_gt);
			}
			result = result && rw->End();
		}

		result = result && rw->WriteInt("th", CClock::GetHour());

		result = result && rw->WriteUns("oc", m_ObjectCount);
		result = result && rw->WriteUns("dcc", m_DrawcallCount);

		if (m_cutsceneFrame > -1)
		{
			result = result && rw->BeginArray("cut", NULL);
			{
				result = result && rw->WriteValue(NULL, m_cutsceneNameHash);
				result = result && rw->WriteValue(NULL, m_cutsceneFrame);
				result = result && rw->WriteValue(NULL, m_cutsceneTotalFrames);
				result = result && rw->WriteValue(NULL, m_cutsceneCurrentConcatSection);
			}
			result = result && rw->End();
		}

		if (CNetworkTelemetry::GetCurrentCheckpoint()>0)
		{
			result = result && rw->WriteUns("chk", CNetworkTelemetry::GetCurrentCheckpoint());
		}

		return result;
	}
};
#endif

class MetricTIMEBARS : public MetricPlayStat
{
	RL_DECLARE_METRIC(TIMEBARS, __tel_logchannel, __tel_logleveldevoff);

public:

	MetricTIMEBARS()
		: m_Timestamp(0)
		, m_Timebar(0)
		, m_Name(0)
		, m_Time(0.0f)
		, m_X(0)
		, m_Y(0)
	{

	}

	explicit MetricTIMEBARS (const u64 timestamp, const unsigned timebar, const unsigned name, const float time, const int X, const int Y) 
		: m_Timestamp(timestamp)
		, m_Timebar(timebar)
		, m_Name(name)
		, m_Time(time)
		, m_X(X)
		, m_Y(Y)
	{
	}

	virtual bool Write(RsonWriter* rw) const
	{
		bool result = this->MetricPlayStat::Write(rw);

		result = result && rw->BeginArray("c", NULL);
		{
			result = result && rw->WriteInt(NULL, m_X);
			result = result && rw->WriteInt(NULL, m_Y);
		}
		result = result && rw->End();

		return result && rw->WriteUns64("ts", m_Timestamp)
			&& rw->WriteUns("id", m_Timebar)
			&& rw->WriteUns("name", m_Name)
			&& rw->WriteFloat("time", m_Time);
	}

private:
	u64       m_Timestamp;
	unsigned  m_Timebar;
	unsigned  m_Name;
	float     m_Time;
	int       m_X;
	int       m_Y;
};

class MetricSTREAMING_REQUEST : public MetricPlayerCoords
{
	RL_DECLARE_METRIC(STREAMING_REQUEST, __tel_logchannel, __tel_loglevel);

public:

	explicit MetricSTREAMING_REQUEST(const int strReq, const bool priority, const bool success)
		: m_StrReq(strReq)
		, m_Priority(priority)
		, m_Success(success)
	{

	}

	virtual bool Write(RsonWriter* rw) const
	{
		bool result = this->MetricPlayerCoords::Write(rw);

		if (m_Priority)
		{
			result = result && rw->WriteBool("pri", m_Priority);
		}

		if (m_Success)
		{
			result = result && rw->WriteBool("succ", m_Success);
		}

		return result;
	}

private:
	int   m_StrReq;
	bool  m_Priority;
	bool  m_Success;

};

class MetricMEMORY_SKELETON : public MetricBUILD_TYPE
{
	RL_DECLARE_METRIC(MEMORY_SKELETON, __tel_logchannel, __tel_logleveldevoff);

private:
	static const unsigned SIZEOF_MEMORY_SKELETON = 5;

public:
	explicit MetricMEMORY_SKELETON(const char* pszMissionName)
	{
		m_mission = atStringHash(pszMissionName);

		m_MemorysSkeleton.clear();
		m_MemorysSkeleton.Reset();
		m_MemorysSkeleton.Resize(SIZEOF_MEMORY_SKELETON);

		m_MemorysSkeleton[0] = 0;
		for (int i = 0; i < CVehicle::GetPool()->GetSize(); ++i)
		{
			CVehicle* pEntity = CVehicle::GetPool()->GetSlot(i);
			if (pEntity && pEntity->GetSkeleton())
				m_MemorysSkeleton[0] += (u32) pEntity->GetSkeleton()->GetMemorySize();
		}

		m_MemorysSkeleton[1] = 0;
		for (int i = 0; i < CPed::GetPool()->GetSize(); ++i)
		{
			CPed* pEntity = CPed::GetPool()->GetSlot(i);
			if (pEntity && pEntity->GetSkeleton())
				m_MemorysSkeleton[1] += (u32) pEntity->GetSkeleton()->GetMemorySize();
		}

		m_MemorysSkeleton[2] = 0;
		for (int i = 0; i < CObject::GetPool()->GetSize(); ++i)
		{
			CObject* pEntity = CObject::GetPool()->GetSlot(i);
			if (pEntity && pEntity->GetSkeleton())
				m_MemorysSkeleton[2] += (u32) pEntity->GetSkeleton()->GetMemorySize();
		}

		m_MemorysSkeleton[3] = 0;
		for (int i = 0; i < CAnimatedBuilding::GetPool()->GetSize(); ++i)
		{
			CAnimatedBuilding* pEntity = CAnimatedBuilding::GetPool()->GetSlot(i);
			if (pEntity && pEntity->GetSkeleton())
				m_MemorysSkeleton[3] += (u32) pEntity->GetSkeleton()->GetMemorySize();
		}

		m_MemorysSkeleton[4] = m_MemorysSkeleton[0] + m_MemorysSkeleton[1] + m_MemorysSkeleton[2] + m_MemorysSkeleton[3];
	}

	~MetricMEMORY_SKELETON()
	{
		m_MemorysSkeleton.clear();
		m_MemorysSkeleton.Reset();
	}

	virtual bool Write(RsonWriter* rw) const
	{
		//Displayf("Mission: 0x%X", m_mission);
		bool success = this->MetricBUILD_TYPE::Write(rw) && rw->WriteUns("m", m_mission);
		const char* pszClass[SIZEOF_MEMORY_SKELETON] = {"v", "p", "o", "b", "t"};

		for (int i = 0; i < m_MemorysSkeleton.GetCount() && success; ++i)
		{	
			//Displayf("Class: %s, Value: %d", pszClass[i], m_MemorysSkeleton[i]);
			success = rw->WriteUns(pszClass[i], m_MemorysSkeleton[i] >> 10);
		}

		return success;
	}

private:
	atFixedArray<u32, SIZEOF_MEMORY_SKELETON> m_MemorysSkeleton;
	u32 m_mission;
};

#if RAGE_POOL_TRACKING
class MetricMEMORY_POOL : public MetricBUILD_TYPE
{
	RL_DECLARE_METRIC(MEMORY_POOL, __tel_logchannel, __tel_logleveldevoff);

public:
	struct sMemPoolInfo
	{
	public:
		sMemPoolInfo() : m_name(0), m_peak(0) {}

		void SetInfo(const char* name, const int peak)
		{
			m_name = 0;
			m_peak = peak;

			if (gnetVerify(name))
				m_name = atStringHash(name);
		}

		sMemPoolInfo& operator=(const sMemPoolInfo& info)
		{
			m_name = info.m_name;
			m_peak = info.m_peak;

			return *this;
		}

	public:
		u32   m_name;
		int   m_peak;
	};
	
	static const unsigned SIZEOF_MEMORY_POOL = 10;

private:
	atFixedArray<sMemPoolInfo, SIZEOF_MEMORY_POOL> m_memoryPool;
	u32 m_mission;

public:
	MetricMEMORY_POOL()
	{
		Clear();
		m_mission = 0;
	}

	MetricMEMORY_POOL(const char* pszMissionName)
	{
		SetName(pszMissionName);
		Clear();
	}

	~MetricMEMORY_POOL() { }

	void SetName(const char* pszMissionName)
	{
		m_mission = atStringHash(pszMissionName);
	}

	void Add(const sMemPoolInfo& info)
	{
		m_memoryPool.Push(info);
	}

	void Clear()
	{
		m_memoryPool.clear();
		m_memoryPool.Reset();
	}

	bool IsEmpty() const
	{
		return m_memoryPool.IsEmpty();
	}

	bool IsFull() const
	{
		return m_memoryPool.IsFull();
	}

	virtual bool Write(RsonWriter* rw) const
	{
		bool success = this->MetricBUILD_TYPE::Write(rw)
			&& rw->WriteUns("m", m_mission) 
			&& rw->BeginArray("#", NULL);

		for (int i = 0; i < m_memoryPool.GetCount() && success; ++i)
		{
			success = rw->Begin(NULL, NULL)
				&& rw->WriteUns("n", m_memoryPool[i].m_name)
				&& rw->WriteInt("p", m_memoryPool[i].m_peak)
				&& rw->End();
		}

		success = success && rw->End();

		return success;
	}
};
#endif // RAGE_POOL_TRACKING

#if ENABLE_STORETRACKING
class MetricMEMORY_STORE : public MetricBUILD_TYPE
{
	RL_DECLARE_METRIC(MEMORY_STORE, __tel_logchannel, __tel_logleveldevoff);

public:
	struct sMemStoreInfo
	{
	public:
		sMemStoreInfo() : m_Used(0), m_Free(0), m_Peak(0), m_ResourceName(0) {;}

		void SetInfo(const char* resourcename, const int used, const int free, const int peak)
		{
			m_Used = used;
			m_Free = free;
			m_Peak = peak;

			m_ResourceName = 0;
			if (gnetVerify(resourcename))
			{
				m_ResourceName = atStringHash( resourcename );
			}
		}

		sMemStoreInfo& operator=(const sMemStoreInfo &rhs)
		{
			m_Used = rhs.m_Used;
			m_Free = rhs.m_Free;
			m_Peak = rhs.m_Peak;
			m_ResourceName = rhs.m_ResourceName;

			return *this;
		}

	public:
		int   m_Used;
		int   m_Free;
		int	  m_Peak;
		u32   m_ResourceName;
	};

private:
	static const unsigned SIZEOF_MEMORY_STORE = 30;

public:
	explicit MetricMEMORY_STORE(const char* pszMissionName, bool debug = false)
	{
		if (0 >= g_storesTrackingArray.GetCount())
			return;

		m_Count = 0;
		m_MemoryStore.clear();
		m_MemoryStore.Reset();
		m_MemoryStore.Resize(SIZEOF_MEMORY_STORE);

		m_mission = atStringHash(pszMissionName);

		gnetAssertf(g_storesTrackingArray.GetCount() <= SIZEOF_MEMORY_STORE, "g_storesTrackingArray.GetCount() = %d", g_storesTrackingArray.GetCount());
		if (debug)
			Displayf("Store, Hash, Size, Used, Peak");

		for (int i=0; i<g_storesTrackingArray.GetCount() && m_Count < SIZEOF_MEMORY_STORE; i++)
		{
			fwAssetStore < char >* temp = (fwAssetStore<char>*)g_storesTrackingArray[i];
			if (gnetVerify(temp) && gnetVerify(temp->GetModuleName()))
			{
				sMemStoreInfo info;
				info.SetInfo(temp->GetModuleName(), temp->GetNumUsedSlots(), temp->GetMaxSize() - temp->GetNumUsedSlots(), temp->GetPeakSlotsUsed());
				m_MemoryStore[m_Count++] = info;

				if (debug)
					Displayf("%s, 0x%X, %d, %d, %d", temp->GetModuleName(), atStringHash(temp->GetModuleName()), temp->GetMaxSize(), temp->GetNumUsedSlots(), temp->GetPeakSlotsUsed());
			}
		}
	}

	~MetricMEMORY_STORE()
	{
		m_MemoryStore.clear();
		m_MemoryStore.Reset();
	}

	virtual bool Write(RsonWriter* rw) const
	{
		bool success = this->MetricBUILD_TYPE::Write(rw)
			&& rw->WriteUns("m", m_mission)
			&& rw->BeginArray("s", NULL);

		for (int i=0; i<m_MemoryStore.GetCount() && success; i++)
		{
			success = rw->Begin(NULL, NULL)
				&& rw->WriteUns("n", m_MemoryStore[i].m_ResourceName)
				&& rw->WriteInt("u", m_MemoryStore[i].m_Used)
				&& rw->WriteInt("f", m_MemoryStore[i].m_Free)
				&& rw->WriteInt("p", m_MemoryStore[i].m_Peak)
				&& rw->End();
		}

		success = success && rw->End();

		return success;
	}

private:
	atFixedArray< sMemStoreInfo, SIZEOF_MEMORY_STORE > m_MemoryStore;
	u32  m_Count;
	u32 m_mission;
};

class MetricMEMORY_USAGE : public MetricBUILD_TYPE
{
	RL_DECLARE_METRIC(MEMORY_USAGE, __tel_logchannel, __tel_logleveldevoff);

public:
	struct sMemUsageInfo
	{
	public:
		sMemUsageInfo() : m_Used(0), m_Free(0), m_Peak(0), m_ResourceName(0) {;}

		void SetInfo(const char* resourcename, const int used, const int free, const int peak)
		{
			m_Used = used;
			m_Free = free;
			m_Peak = peak;

			m_ResourceName = 0;
			if (gnetVerify(resourcename))
			{
				m_ResourceName = atStringHash( resourcename );
			}
		}

		sMemUsageInfo& operator=(const sMemUsageInfo &rhs)
		{
			m_Used = rhs.m_Used;
			m_Free = rhs.m_Free;
			m_Peak = rhs.m_Peak;
			m_ResourceName = rhs.m_ResourceName;

			return *this;
		}

	public:
		int   m_Used;
		int   m_Free;
		int	  m_Peak;
		u32   m_ResourceName;
	};

private:
	static const unsigned SIZEOF_MEMORY_USAGE = 13;

public:
	explicit MetricMEMORY_USAGE(const char* pszMissionName, bool debug = false)
	{
		m_MemoryUsage.clear();
		m_MemoryUsage.Reset();
		m_MemoryUsage.Resize(SIZEOF_MEMORY_USAGE);

		if (debug)
			Displayf("Memory Usage, Name, Hash, Total, Used, Free, Peak");

		unsigned idx = 0;
		sMemUsageInfo memUsage;

		m_mission = atStringHash(pszMissionName);

		sysMemAllocator* pAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL);
		memUsage.SetInfo("v_game", (s32)pAllocator->GetMemoryUsed()/1024, (s32)pAllocator->GetMemoryAvailable()/1024, (s32) pAllocator->GetHighWaterMark(false)/1024);
		m_MemoryUsage[idx++] = memUsage;
		if (debug)
			Displayf("%s, 0x%X, %d, %d, %d, %d", "v_game", memUsage.m_ResourceName, memUsage.m_Used + memUsage.m_Free, memUsage.m_Used, memUsage.m_Free, memUsage.m_Peak);

		pAllocator = fragManager::GetFragCacheAllocator();
		if (pAllocator)
		{
			memUsage.SetInfo("v_frag", (s32)pAllocator->GetMemoryUsed()/1024, (s32)pAllocator->GetMemoryAvailable()/1024, (s32) pAllocator->GetHighWaterMark(false)/1024);
			m_MemoryUsage[idx++] = memUsage;
			if (debug)
				Displayf("%s, 0x%X, %d, %d, %d, %d", "v_frag", memUsage.m_ResourceName, memUsage.m_Used + memUsage.m_Free, memUsage.m_Used, memUsage.m_Free, memUsage.m_Peak);
		}		

		pAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_VIRTUAL);
		s32 freeVirtualResource = (s32)pAllocator->GetMemoryAvailable();

		memUsage.SetInfo("v_resource", (s32)pAllocator->GetMemoryUsed()/1024, freeVirtualResource/1024, (s32) pAllocator->GetHighWaterMark(false)/1024);
		m_MemoryUsage[idx++] = memUsage;
		if (debug) 
		{
			Displayf("%s, 0x%X, %d, %d, %d, %d", "v_resource", memUsage.m_ResourceName, memUsage.m_Used + memUsage.m_Free, memUsage.m_Used, memUsage.m_Free, memUsage.m_Peak);
		}

		s32 usedStreaming = (s32) strStreamingEngine::GetAllocator().GetPhysicalMemoryUsed();
		s32 availableStreaming = (s32) strStreamingEngine::GetAllocator().GetPhysicalMemoryAvailable();
		s32 freeStreaming = availableStreaming - usedStreaming;

		memUsage.SetInfo("v_streaming", usedStreaming >> 10, freeStreaming >> 10, 0);
		m_MemoryUsage[idx++] = memUsage;
		if (debug)
			Displayf("%s, 0x%X, %d, %d, %d, %d", "v_streaming", memUsage.m_ResourceName, memUsage.m_Used + memUsage.m_Free, memUsage.m_Used, memUsage.m_Free, memUsage.m_Peak);

		memUsage.SetInfo("v_resource_slush", (freeVirtualResource - freeStreaming)/1024, 0, 0);
		m_MemoryUsage[idx++] = memUsage;
		if (debug)
			Displayf("%s, 0x%X, %d, %d, %d, %d", "v_resource_slush", memUsage.m_ResourceName, memUsage.m_Used + memUsage.m_Free, memUsage.m_Used, memUsage.m_Free, memUsage.m_Peak);

		sysMemSimpleAllocator* simpleAlloc = static_cast<sysMemSimpleAllocator*>(pAllocator); 
		size_t totalSize = simpleAlloc->GetSmallocatorTotalSize();

		memUsage.SetInfo("v_smallocator", (s32)totalSize/1024, 0, 0);
		m_MemoryUsage[idx++] = memUsage;
		if (debug)
			Displayf("%s, 0x%X, %d, %d, %d, %d", "v_smallocator", memUsage.m_ResourceName, memUsage.m_Used + memUsage.m_Free, memUsage.m_Used, memUsage.m_Free, memUsage.m_Peak);

		// Archetypes
#if __BANK
		fwArchetypePool& ap = fwArchetypeManager::GetPool();
		memUsage.SetInfo("perm arch", (s32) ap.GetNoOfUsedSpaces(), (s32) ap.GetNoOfFreeSpaces(), (s32) ap.GetPeakSlotsUsed());
		m_MemoryUsage[idx++] = memUsage;
		if (debug)
			Displayf("%s, 0x%X, %d, %d, %d, %d", "perm arch", memUsage.m_ResourceName, memUsage.m_Used + memUsage.m_Free, memUsage.m_Used, memUsage.m_Free, memUsage.m_Peak);
#endif
	}

	~MetricMEMORY_USAGE()
	{
		m_MemoryUsage.clear();
		m_MemoryUsage.Reset();
	}

	virtual bool Write(RsonWriter* rw) const
	{
		bool success = this->MetricBUILD_TYPE::Write(rw)
			&& rw->WriteUns("m", m_mission) 
			&& rw->BeginArray("usage", NULL);

		for (int i=0; i<m_MemoryUsage.GetCount() && success; i++)
		{
			success = rw->Begin(NULL, NULL)
				&& rw->WriteUns("n", m_MemoryUsage[i].m_ResourceName) 
				&& rw->WriteInt("u", m_MemoryUsage[i].m_Used) 
				&& rw->WriteInt("f", m_MemoryUsage[i].m_Free)
				&& rw->WriteInt("p", m_MemoryUsage[i].m_Peak)
				&& rw->End();
		}

		success = success && rw->End();

		return success;
	}


private:
	atFixedArray< sMemUsageInfo, SIZEOF_MEMORY_USAGE > m_MemoryUsage;
	u32 m_mission;
};
#endif // ENABLE_STORETRACKING

class MetricCONSOLE_MEMORY : public MetricPlayStat
{
	RL_DECLARE_METRIC(CONSOLE_MEMORY, __tel_logchannel, __tel_logleveldevoff);

public:

	virtual bool Write(RsonWriter* rw) const
	{
		return this->MetricPlayStat::Write(rw) 
			&& AppendMetric_IP_MAC_ADDRESS(rw)
			ENABLE_DEBUG_HEAP_ONLY(&& rw->WriteBool("1gb", g_sysHasDebugHeap))
			&& AppendMetric_BUILD_TYPE(rw);
	}
};

#if ENABLE_STATS_CAPTURE 

namespace SmokeTests
{
	XPARAM(automatedtest);
};

// Packet Definitions
class MetricCAPTURE_ID : public MetricPlayerCoords
{
	RL_DECLARE_METRIC(CAPTURE_ID, TELEMETRY_CHANNEL_DEV_CAPTURE, __tel_loglevel);

public:

	explicit MetricCAPTURE_ID(atString &zoneName, int idx)
	{
		m_ZoneHash = atStringHash(zoneName);
		m_index = idx;
	}

	virtual bool Write(RsonWriter* rw) const
	{
		int autoTestID = 0;
		int	tempTestID = 0;
		if(	SmokeTests::PARAM_automatedtest.Get(tempTestID) )
		{
			autoTestID = tempTestID;
		}

		return this->MetricPlayerCoords::Write(rw)
			&& rw->WriteUns("zone", m_ZoneHash)		// Header.. Zone Name Hash
			&& rw->WriteInt("cidx", m_index)		// Capture index
			&& rw->WriteInt("atst", autoTestID);	// automatedtest number (if it exists, see B*834787)
	}

	u32						m_ZoneHash;
	int						m_index;
};

class MetricCAPTURE_FPS : public MetricCAPTURE_ID
{
	RL_DECLARE_METRIC(CAPTURE_FPS, TELEMETRY_CHANNEL_DEV_CAPTURE, __tel_logleveldevoff);

public:

	explicit MetricCAPTURE_FPS(atString &zoneName, int idx, framesPerSecondResult &result) : MetricCAPTURE_ID(zoneName, idx)
	{
		m_Result = result;
	}

	virtual bool Write(RsonWriter* rw) const
	{
		return this->MetricCAPTURE_ID::Write(rw)
			&& rw->WriteFloat("min", m_Result.min)	// Data
			&& rw->WriteFloat("max", m_Result.max)
			&& rw->WriteFloat("avg", m_Result.average)
			&& rw->WriteFloat("std", m_Result.std);
	}

	framesPerSecondResult	m_Result;
};

class MetricCAPTURE_MSPERFRAME : public MetricCAPTURE_ID
{
	RL_DECLARE_METRIC(CAPTURE_MSPERFRAME, TELEMETRY_CHANNEL_DEV_CAPTURE, __tel_logleveldevoff);

public:

	explicit MetricCAPTURE_MSPERFRAME(atString &zoneName, int idx, msPerFrameResult &result) : MetricCAPTURE_ID(zoneName, idx)
	{
		m_Result = result;
	}

	virtual bool Write(RsonWriter* rw) const
	{
		return this->MetricCAPTURE_ID::Write(rw)
			&& rw->WriteFloat("min", m_Result.min)	// Data
			&& rw->WriteFloat("max", m_Result.max)
			&& rw->WriteFloat("avg", m_Result.average)
			&& rw->WriteFloat("std", m_Result.std);
	}

	msPerFrameResult	m_Result;
};

class MetricCAPTURE_GPU : public MetricCAPTURE_ID
{
	RL_DECLARE_METRIC(CAPTURE_GPU, TELEMETRY_CHANNEL_DEV_CAPTURE, __tel_logleveldevoff);

public:

	explicit MetricCAPTURE_GPU(atString &zoneName, int idx, gpuTimingResult &result) : MetricCAPTURE_ID(zoneName, idx)
	{
		idx = result.idx;
		name = result.name;
		time = result.time;
	}

	virtual bool Write(RsonWriter* rw) const
	{
		return this->MetricCAPTURE_ID::Write(rw)
			&& rw->WriteInt("idx", idx)	// Data
			&& rw->WriteUns("name", name.GetHash())
			&& rw->WriteFloat("time", time);
	}

	int idx;
	atHashString name;
	float time;
};

class MetricCAPTURE_CPU : public MetricCAPTURE_ID
{
	RL_DECLARE_METRIC(CAPTURE_CPU, TELEMETRY_CHANNEL_DEV_CAPTURE, __tel_logleveldevoff);

public:

	explicit MetricCAPTURE_CPU(atString &zoneName, int idx, cpuTimingResult &result) : MetricCAPTURE_ID(zoneName, idx)
	{
		idx = result.idx;
		name = result.name;
		set = result.set;
		min = result.min;
		max = result.max;
		average = result.average;
		std = result.std;
	}

	virtual bool Write(RsonWriter* rw) const
	{
		return this->MetricCAPTURE_ID::Write(rw)
			&& rw->WriteInt("idx", idx)	// Data
			&& rw->WriteUns("name", name.GetHash())
			&& rw->WriteUns("set", set.GetHash())
			&& rw->WriteFloat("min", min)
			&& rw->WriteFloat("max", max)
			&& rw->WriteFloat("avg", average)
			&& rw->WriteFloat("std", std);
	}

	int idx;
	atHashString name;
	atHashString set;
	float min;
	float max;
	float average;
	float std;
};

class MetricCAPTURE_THREAD : public MetricCAPTURE_ID
{
	RL_DECLARE_METRIC(CAPTURE_THREAD, TELEMETRY_CHANNEL_DEV_CAPTURE, __tel_logleveldevoff);

public:

	explicit MetricCAPTURE_THREAD(atString &zoneName, int idx, threadTimingResult &result) : MetricCAPTURE_ID(zoneName, idx)
	{
		idx = result.idx;
		name = result.name;
		min = result.min;
		max = result.max;
		average = result.average;
		std = result.std;
	}

	virtual bool Write(RsonWriter* rw) const
	{
		return this->MetricCAPTURE_ID::Write(rw)
			&& rw->WriteInt("idx", idx)	// Data
			&& rw->WriteUns("name", name.GetHash())
			&& rw->WriteFloat("min", min)
			&& rw->WriteFloat("max", max)
			&& rw->WriteFloat("avg", average)
			&& rw->WriteFloat("std", std);
	}

	int idx;
	atHashString name;
	float min;
	float max;
	float average;
	float std;
};

class MetricCAPTURE_MEMBUCKET : public MetricCAPTURE_ID
{
	RL_DECLARE_METRIC(CAPTURE_MEMBUCKET, TELEMETRY_CHANNEL_DEV_CAPTURE, __tel_logleveldevoff);

public:

	explicit MetricCAPTURE_MEMBUCKET(atString &zoneName, int idx, memoryBucketUsageResult &result) : MetricCAPTURE_ID(zoneName, idx)
	{
		idx = result.idx;
		name = result.name;
		gameVirtual = result.gameVirtual;
		gamePhysical = result.gamePhysical;
		resourceVirtual = result.resourceVirtual;
		resourcePhysical = result.resourcePhysical;
	}

	virtual bool Write(RsonWriter* rw) const
	{
		return this->MetricCAPTURE_ID::Write(rw)

			&& rw->WriteInt("idx", idx)	// Data
			&& rw->WriteUns("name", name.GetHash())

			&& rw->Begin("gameVirt", NULL)
			&& rw->WriteInt("min", gameVirtual.min)
			&& rw->WriteInt("max", gameVirtual.max)
			&& rw->WriteInt("avg", gameVirtual.average)
			&& rw->WriteFloat("std", gameVirtual.std)
			&& rw->End()

			&& rw->Begin("gamePhys", NULL)
			&& rw->WriteInt("min", gamePhysical.min)
			&& rw->WriteInt("max", gamePhysical.max)
			&& rw->WriteInt("avg", gamePhysical.average)
			&& rw->WriteFloat("std", gamePhysical.std)
			&& rw->End()

			&& rw->Begin("resVirt", NULL)
			&& rw->WriteInt("min", resourceVirtual.min)
			&& rw->WriteInt("max", resourceVirtual.max)
			&& rw->WriteInt("avg", resourceVirtual.average)
			&& rw->WriteFloat("std", resourceVirtual.std)
			&& rw->End()

			&& rw->Begin("resPhys", NULL)
			&& rw->WriteInt("min", resourcePhysical.min)
			&& rw->WriteInt("max", resourcePhysical.max)
			&& rw->WriteInt("avg", resourcePhysical.average)
			&& rw->WriteFloat("std", resourcePhysical.std)
			&& rw->End();
	}

	int idx;
	atHashString name;
	memoryUsageResult gameVirtual;
	memoryUsageResult gamePhysical;
	memoryUsageResult resourceVirtual;
	memoryUsageResult resourcePhysical;
};

class MetricCAPTURE_STREAMMEM : public MetricCAPTURE_ID
{
	RL_DECLARE_METRIC(CAPTURE_STREAMMEM, TELEMETRY_CHANNEL_DEV_CAPTURE, __tel_logleveldevoff);

public:

	typedef struct
	{
		atHashString	categoryName;
		u32				virtualMemory;
		u32				physicalMemory;
	} streamingMemoryCategoryResultTelemetry;

	explicit MetricCAPTURE_STREAMMEM(atString &zoneName, int idx, streamingMemoryResult &result) : MetricCAPTURE_ID(zoneName, idx)
	{
		moduleName = result.moduleName;
		for(int i=0;i<CDataFileMgr::CONTENTS_MAX;i++)
		{
			categories[i].categoryName = result.categories[i].categoryName;
			categories[i].physicalMemory = result.categories[i].physicalMemory;
			categories[i].virtualMemory = result.categories[i].virtualMemory;
		}
	}

	virtual bool Write(RsonWriter* rw) const
	{
		bool success = this->MetricCAPTURE_ID::Write(rw)
			&& rw->WriteUns("mod", moduleName.GetHash())
			&& rw->BeginArray("cats", NULL);

		for(int i=0;i<CDataFileMgr::CONTENTS_MAX && success;i++)
		{
			success = rw->Begin(categories[i].categoryName.GetCStr(), NULL)
				&& rw->WriteInt("virt", categories[i].virtualMemory )
				&& rw->WriteInt("phys", categories[i].physicalMemory )
				&& rw->End();
		}

		return success && rw->End();
	}

	atHashString moduleName;
	streamingMemoryCategoryResultTelemetry categories[ CDataFileMgr::CONTENTS_MAX ];
};


class MetricCAPTURE_DRAWLIST : public MetricCAPTURE_ID
{
	RL_DECLARE_METRIC(CAPTURE_DRAWLIST, TELEMETRY_CHANNEL_DEV_CAPTURE, __tel_logleveldevoff);

public:

	explicit MetricCAPTURE_DRAWLIST(atString &zoneName, int idx, drawListResult &result) : MetricCAPTURE_ID(zoneName, idx)
	{
		idx = result.idx;
		name = result.name;
		min = result.min;
		max = result.max;
		average = result.average;
		std = result.std;
	}

	virtual bool Write(RsonWriter* rw) const
	{
		return this->MetricCAPTURE_ID::Write(rw)

			&& rw->WriteInt("idx", idx)
			&& rw->WriteUns("name", name.GetHash())
			&& rw->WriteInt("min", min)
			&& rw->WriteInt("max", max)
			&& rw->WriteInt("avg", average)
			&& rw->WriteFloat("std", std);
	}

	int idx;
	atHashString name;
	int min;
	int max;
	int average;
	float std;
};

class MetricCAPTURE_MEMDIST : public MetricCAPTURE_ID
{
	RL_DECLARE_METRIC(CAPTURE_MEMDIST, TELEMETRY_CHANNEL_DEV_CAPTURE, __tel_logleveldevoff);

public:

	explicit MetricCAPTURE_MEMDIST(atString &zoneName, int idx, memoryDistributionResult &result) : MetricCAPTURE_ID(zoneName, idx)
	{
		m_Result = result;
	}

	virtual bool Write(RsonWriter* rw) const
	{
		return this->MetricCAPTURE_ID::Write(rw)

			&& rw->WriteInt("idx", m_Result.idx)

			&& rw->Begin("physUsed",NULL)
			&& rw->WriteInt("min", m_Result.physicalUsedBySize.min)
			&& rw->WriteInt("max", m_Result.physicalUsedBySize.max)
			&& rw->WriteInt("avg", m_Result.physicalUsedBySize.average)
			&& rw->WriteFloat("std", m_Result.physicalUsedBySize.std)
			&& rw->End()

			&& rw->Begin("physFree",NULL)
			&& rw->WriteInt("min", m_Result.physicalFreeBySize.min)
			&& rw->WriteInt("max", m_Result.physicalFreeBySize.max)
			&& rw->WriteInt("avg", m_Result.physicalFreeBySize.average)
			&& rw->WriteFloat("std", m_Result.physicalFreeBySize.std)
			&& rw->End()

			&& rw->Begin("virtUsed",NULL)
			&& rw->WriteInt("min", m_Result.virtualUsedBySize.min)
			&& rw->WriteInt("max", m_Result.virtualUsedBySize.max)
			&& rw->WriteInt("avg", m_Result.virtualUsedBySize.average)
			&& rw->WriteFloat("std", m_Result.virtualUsedBySize.std)
			&& rw->End()

			&& rw->Begin("virtFree",NULL)
			&& rw->WriteInt("min", m_Result.virtualFreeBySize.min)
			&& rw->WriteInt("max", m_Result.virtualFreeBySize.max)
			&& rw->WriteInt("avg", m_Result.virtualFreeBySize.average)
			&& rw->WriteFloat("std", m_Result.virtualFreeBySize.std)
			&& rw->End();
	}

	memoryDistributionResult	m_Result;
};

class MetricCAPTURE_SCRIPTMEM : public MetricCAPTURE_ID
{
	RL_DECLARE_METRIC(CAPTURE_SCRIPTMEM, TELEMETRY_CHANNEL_DEV_CAPTURE, __tel_logleveldevoff);

public:

	explicit MetricCAPTURE_SCRIPTMEM(atString &zoneName, int idx, scriptMemoryUsageResult &result) : MetricCAPTURE_ID(zoneName, idx)
	{
		m_Result = result;
	}

	virtual bool Write(RsonWriter* rw) const
	{
		return this->MetricCAPTURE_ID::Write(rw)

			&& rw->Begin("physMem", NULL)
			&& rw->WriteInt("min", m_Result.physicalMem.min)
			&& rw->WriteInt("max", m_Result.physicalMem.max)
			&& rw->WriteInt("avg", m_Result.physicalMem.average)
			&& rw->WriteFloat("std", m_Result.physicalMem.std)
			&& rw->End()

			&& rw->Begin("virtMem", NULL)
			&& rw->WriteInt("min", m_Result.virtualMem.min)
			&& rw->WriteInt("max", m_Result.virtualMem.max)
			&& rw->WriteInt("avg", m_Result.virtualMem.average)
			&& rw->WriteFloat("std", m_Result.virtualMem.std)
			&& rw->End();
	}

	scriptMemoryUsageResult	m_Result;
};


class MetricCAPTURE_PEDANDVEHICLESTAT : public MetricCAPTURE_ID
{

	RL_DECLARE_METRIC(CAPTURE_PEDANDVEHICLESTAT, TELEMETRY_CHANNEL_DEV_CAPTURE, __tel_logleveldevoff);

public:

	explicit MetricCAPTURE_PEDANDVEHICLESTAT(atString &zoneName, int idx, pedAndVehicleStatResults &result) : MetricCAPTURE_ID(zoneName, idx)
	{
		m_Result = result;
	}

	virtual bool Write(RsonWriter* rw) const
	{
		return this->MetricCAPTURE_ID::Write(rw)

			&& rw->Begin("numPeds", NULL)
			&& rw->WriteInt("min", m_Result.numPeds.min)
			&& rw->WriteInt("max", m_Result.numPeds.max)
			&& rw->WriteInt("avg", m_Result.numPeds.average)
			&& rw->End()

			&& rw->Begin("activePeds", NULL)
			&& rw->WriteInt("min", m_Result.numActivePeds.min)
			&& rw->WriteInt("max", m_Result.numActivePeds.max)
			&& rw->WriteInt("avg", m_Result.numActivePeds.average)
			&& rw->End()

			&& rw->Begin("inactivePeds", NULL)
			&& rw->WriteInt("min", m_Result.numInactivePeds.min)
			&& rw->WriteInt("max", m_Result.numInactivePeds.max)
			&& rw->WriteInt("avg", m_Result.numInactivePeds.average)
			&& rw->End()

			&& rw->Begin("eventScanHiPeds", NULL)
			&& rw->WriteInt("min", m_Result.numEventScanHiPeds.min)
			&& rw->WriteInt("max", m_Result.numEventScanHiPeds.max)
			&& rw->WriteInt("avg", m_Result.numEventScanHiPeds.average)
			&& rw->End()

			&& rw->Begin("eventScanLoPeds", NULL)
			&& rw->WriteInt("min", m_Result.numEventScanLoPeds.min)
			&& rw->WriteInt("max", m_Result.numEventScanLoPeds.max)
			&& rw->WriteInt("avg", m_Result.numEventScanLoPeds.average)
			&& rw->End()

			&& rw->Begin("motionTaskHiPeds", NULL)
			&& rw->WriteInt("min", m_Result.numMotionTaskHiPeds.min)
			&& rw->WriteInt("max", m_Result.numMotionTaskHiPeds.max)
			&& rw->WriteInt("avg", m_Result.numMotionTaskHiPeds.average)
			&& rw->End()

			&& rw->Begin("motionTaskLoPeds", NULL)
			&& rw->WriteInt("min", m_Result.numMotionTaskLoPeds.min)
			&& rw->WriteInt("max", m_Result.numMotionTaskLoPeds.max)
			&& rw->WriteInt("avg", m_Result.numMotionTaskLoPeds.average)
			&& rw->End()

			&& rw->Begin("physicsHiPeds", NULL)
			&& rw->WriteInt("min", m_Result.numPhysicsHiPeds.min)
			&& rw->WriteInt("max", m_Result.numPhysicsHiPeds.max)
			&& rw->WriteInt("avg", m_Result.numPhysicsHiPeds.average)
			&& rw->End()

			&& rw->Begin("physicsLoPeds", NULL)
			&& rw->WriteInt("min", m_Result.numPhysicsLoPeds.min)
			&& rw->WriteInt("max", m_Result.numPhysicsLoPeds.max)
			&& rw->WriteInt("avg", m_Result.numPhysicsLoPeds.average)
			&& rw->End()

			&& rw->Begin("entityScanHiPeds", NULL)
			&& rw->WriteInt("min", m_Result.numEntityScanHiPeds.min)
			&& rw->WriteInt("max", m_Result.numEntityScanHiPeds.max)
			&& rw->WriteInt("avg", m_Result.numEntityScanHiPeds.average)
			&& rw->End()

			&& rw->Begin("entityScanLoPeds", NULL)
			&& rw->WriteInt("min", m_Result.numEntityScanLoPeds.min)
			&& rw->WriteInt("max", m_Result.numEntityScanLoPeds.max)
			&& rw->WriteInt("avg", m_Result.numEntityScanLoPeds.average)
			&& rw->End()

			&& rw->Begin("numVehs", NULL)
			&& rw->WriteInt("min", m_Result.numVehicles.min)
			&& rw->WriteInt("max", m_Result.numVehicles.max)
			&& rw->WriteInt("avg", m_Result.numVehicles.average)
			&& rw->End()

			&& rw->Begin("activeVehs", NULL)
			&& rw->WriteInt("min", m_Result.numActiveVehicles.min)
			&& rw->WriteInt("max", m_Result.numActiveVehicles.max)
			&& rw->WriteInt("avg", m_Result.numActiveVehicles.average)
			&& rw->End()

			&& rw->Begin("inactiveVehs", NULL)
			&& rw->WriteInt("min", m_Result.numInactiveVehicles.min)
			&& rw->WriteInt("max", m_Result.numInactiveVehicles.max)
			&& rw->WriteInt("avg", m_Result.numInactiveVehicles.average)
			&& rw->End()

			&& rw->Begin("realVehs", NULL)
			&& rw->WriteInt("min", m_Result.numRealVehicles.min)
			&& rw->WriteInt("max", m_Result.numRealVehicles.max)
			&& rw->WriteInt("avg", m_Result.numRealVehicles.average)
			&& rw->End()

			&& rw->Begin("dummyVehs", NULL)
			&& rw->WriteInt("min", m_Result.numDummyVehicles.min)
			&& rw->WriteInt("max", m_Result.numDummyVehicles.max)
			&& rw->WriteInt("avg", m_Result.numDummyVehicles.average)
			&& rw->End()

			&& rw->Begin("superDummyVehs", NULL)
			&& rw->WriteInt("min", m_Result.numSuperDummyVehicles.min)
			&& rw->WriteInt("max", m_Result.numSuperDummyVehicles.max)
			&& rw->WriteInt("avg", m_Result.numSuperDummyVehicles.average)
			&& rw->End();
	}

	pedAndVehicleStatResults	m_Result;
};


// Storage
atArray<CNetworkDebugTelemetry::AutoGPUCaptureTelemetryStore> CNetworkDebugTelemetry::m_AutoGPUDataStore;

// Copy the data into storage
CNetworkDebugTelemetry::AutoGPUCaptureTelemetryStore& CNetworkDebugTelemetry::AutoGPUCaptureTelemetryStore::operator =(const metricsList &other)
{

	idx = other.idx;
	name = other.name;

	// FPS RESULT
	fpsResults.ResizeGrow(1);
	fpsResults[0] = *(other.fpsResult);

	// MS PER FRAME RESULT
	msPerFrameResults.ResizeGrow(1);
	msPerFrameResults[0] = *(other.msPFResult);

	// GPU RESULTS
	gpuResults.ResizeGrow( other.gpuResults.size() );
	for(int i=0;i<other.gpuResults.size();i++)
	{
		gpuResults[i] = *(other.gpuResults[i]);
	}

	// CPU RESULTS
	cpuResults.ResizeGrow( other.cpuResults.size() );
	for(int i=0;i<other.cpuResults.size();i++)
	{
		cpuResults[i] = *(other.cpuResults[i]);
	}

	// THREAD RESULTS
	threadResults.ResizeGrow(other.threadResults.size() );
	for(int i=0;i<other.threadResults.size();i++)
	{
		threadResults[i] = *(other.threadResults[i]);
	}

	// MEMORY RESULTS
	memoryResults.ResizeGrow(other.memoryResults.size() );
	for(int i=0;i<other.memoryResults.size();i++)
	{
		memoryResults[i] = *(other.memoryResults[i]);
	}

	// STREAMING MEMORY RESULTS
	streamingMemoryResults.ResizeGrow(other.streamingMemoryResults.size() );
	for(int i=0;i<other.streamingMemoryResults.size();i++)
	{
		streamingMemoryResults[i] = *(other.streamingMemoryResults[i]);
	}

	// DRAWLIST RESUTLS
	drawListResults.ResizeGrow(other.drawListResults.size() );
	for(int i=0;i<other.drawListResults.size();i++)
	{
		drawListResults[i] = *(other.drawListResults[i]);
	}

	// MEM DISTRIBUTION RESULTS
	memDistributionResults.ResizeGrow(other.memDistributionResults.size() );
	for(int i=0;i<other.memDistributionResults.size();i++)
	{
		memDistributionResults[i] = *(other.memDistributionResults[i]);
	}

	// SCRIPT MEMORY RESULTS
	scriptMemResults.ResizeGrow(1);
	scriptMemResults[0] = *(other.scriptMemResult);

	// PED AND VEHICLE STATS
	pedAndVehicleResults.ResizeGrow(1);
	pedAndVehicleResults[0] = *(other.pedAndVehicleResults);

	return *this;
}

bool CNetworkDebugTelemetry::AutoGPUCaptureTelemetryStore::IsEmpty()
{
	bool	res = true;

	if( fpsResults.size() ||
		msPerFrameResults.size() ||
		gpuResults.size() ||
		cpuResults.size() ||
		threadResults.size() ||
		memoryResults.size() ||
		streamingMemoryResults.size() ||
		drawListResults.size() ||
		memDistributionResults.size() ||
		scriptMemResults.size() ||
		pedAndVehicleResults.size() )
	{
		res = false;
	}
	return res;
}


void CNetworkDebugTelemetry::AddAutoGPUTelemetry( metricsList *pList )
{
	USE_DEBUG_MEMORY();
	m_AutoGPUDataStore.Grow() = *pList;
}

bool CNetworkDebugTelemetry::AppendMetricNextAutoGPUPacket()
{
	bool	success = false;

	if( m_AutoGPUDataStore.size() )
	{
		AutoGPUCaptureTelemetryStore &thisStore = m_AutoGPUDataStore[0];

		// FPS RESULTS
		if( thisStore.fpsResults.size() )
		{
			// Write out a FPS result
			framesPerSecondResult &result = thisStore.fpsResults[0];							// Get the result
			MetricCAPTURE_FPS m(thisStore.name, thisStore.idx, result);	// Create the metric

			// Try to add the metric
			bool success = AppendDebugMetric(&m);
			gnetAssertf(success, "Failed to append Debug telemetry metric '%s'", m.GetMetricName());
			if( success )
			{
				USE_DEBUG_MEMORY(); // used in this scope rather than globally, because I don't want to bust CNetworkTelemetry::AppendMetric()
				// Delete this record now it's been successfully processed.
				thisStore.fpsResults.Delete(0);		
			}
		}
		// MS PER FRAME RESULTS
		if( thisStore.msPerFrameResults.size() )
		{
			// Write out a FPS result
			msPerFrameResult &result = thisStore.msPerFrameResults[0];							// Get the result
			MetricCAPTURE_MSPERFRAME m(thisStore.name, thisStore.idx, result);	// Create the metric

			// Try to add the metric
			bool success = AppendDebugMetric(&m);
			gnetAssertf(success, "Failed to append Debug telemetry metric '%s'", m.GetMetricName());
			if( success )
			{
				USE_DEBUG_MEMORY(); // used in this scope rather than globally, because I don't want to bust CNetworkTelemetry::AppendMetric()
				// Delete this record now it's been successfully processed.
				thisStore.msPerFrameResults.Delete(0);		
			}
		}
		// GPU RESULTS
		else if(thisStore.gpuResults.size())
		{
			// Write out a GPU result
			gpuTimingResult &result = thisStore.gpuResults[0];
			MetricCAPTURE_GPU m(thisStore.name, thisStore.idx, result);

			// Try to add the metric
			bool success = AppendDebugMetric(&m);
			gnetAssertf(success, "Failed to append Debug telemetry metric '%s'", m.GetMetricName());
			if( success )
			{
				USE_DEBUG_MEMORY();
				// Delete this record now it's been successfully processed.
				thisStore.gpuResults.Delete(0);		
			}
		}
		// CPU RESULTS
		else if(thisStore.cpuResults.size())
		{
			// Write out a CPU result
			cpuTimingResult &result = thisStore.cpuResults[0];
			MetricCAPTURE_CPU m(thisStore.name, thisStore.idx, result);

			// Try to add the metric
			bool success = AppendDebugMetric(&m);
			gnetAssertf(success, "Failed to append Debug telemetry metric '%s'", m.GetMetricName());
			if( success )
			{
				USE_DEBUG_MEMORY();
				// Delete this record now it's been successfully processed.
				thisStore.cpuResults.Delete(0);		
			}
		}
		// THREAD RESULTS
		else if(thisStore.threadResults.size())
		{
			// Write out a THREAD result
			threadTimingResult &result = thisStore.threadResults[0];
			MetricCAPTURE_THREAD m(thisStore.name, thisStore.idx, result);

			// Try to add the metric
			bool success = AppendDebugMetric(&m);
			gnetAssertf(success, "Failed to append Debug telemetry metric '%s'", m.GetMetricName());
			if( success )
			{
				USE_DEBUG_MEMORY();
				// Delete this record now it's been successfully processed.
				thisStore.threadResults.Delete(0);		
			}
		}
		// MEMORY BUCKET RESULTS
		else if(thisStore.memoryResults.size())
		{
			// Write out a MEMBUCKET result
			memoryBucketUsageResult &result = thisStore.memoryResults[0];
			MetricCAPTURE_MEMBUCKET m(thisStore.name, thisStore.idx, result);

			// Try to add the metric
			bool success = AppendDebugMetric(&m);
			gnetAssertf(success, "Failed to append Debug telemetry metric '%s'", m.GetMetricName());
		
			if( success )
			{
				USE_DEBUG_MEMORY();
				// Delete this record now it's been successfully processed.
				thisStore.memoryResults.Delete(0);		
			}
		}
		// STREAMING MEMORY RESULTS
		else if(thisStore.streamingMemoryResults.size())
		{
			// Write out a MEMBUCKET result
			streamingMemoryResult &result = thisStore.streamingMemoryResults[0];
			MetricCAPTURE_STREAMMEM m(thisStore.name, thisStore.idx, result);

			// Try to add the metric
			bool success = AppendDebugMetric(&m);
			gnetAssertf(success, "Failed to append Debug telemetry metric '%s'", m.GetMetricName());

			if( success )
			{
				USE_DEBUG_MEMORY();
				// Delete this record now it's been successfully processed.
				thisStore.streamingMemoryResults.Delete(0);		
			}
		}
		// STREAMING MEMORY RESULTS
		else if(thisStore.drawListResults.size())
		{
			// Write out a MEMBUCKET result
			drawListResult &result = thisStore.drawListResults[0];
			MetricCAPTURE_DRAWLIST m(thisStore.name, thisStore.idx, result);

			// Try to add the metric
			bool success = AppendDebugMetric(&m);
			gnetAssertf(success, "Failed to append Debug telemetry metric '%s'", m.GetMetricName());
	
			if( success )
			{
				USE_DEBUG_MEMORY();
				// Delete this record now it's been successfully processed.
				thisStore.drawListResults.Delete(0);		
			}
		}
		// MEMORY DISTRIBUTION RESULTS
		else if(thisStore.memDistributionResults.size())
		{
			// Write out a MEMBUCKET result
			memoryDistributionResult &result = thisStore.memDistributionResults[0];
			MetricCAPTURE_MEMDIST m(thisStore.name, thisStore.idx, result);

			// Try to add the metric
			bool success = AppendDebugMetric(&m);
			gnetAssertf(success, "Failed to append Debug telemetry metric '%s'", m.GetMetricName());

			if( success )
			{
				USE_DEBUG_MEMORY();
				// Delete this record now it's been successfully processed.
				thisStore.memDistributionResults.Delete(0);		
			}
		}
		// SCRIPT MEMORY RESULTS
		else if(thisStore.scriptMemResults.size())
		{
			// Write out a MEMBUCKET result
			scriptMemoryUsageResult &result = thisStore.scriptMemResults[0];
			MetricCAPTURE_SCRIPTMEM m(thisStore.name, thisStore.idx, result);

			// Try to add the metric
			bool success = AppendDebugMetric(&m);
			gnetAssertf(success, "Failed to append Debug telemetry metric '%s'", m.GetMetricName());

			if( success )
			{
				USE_DEBUG_MEMORY();
				// Delete this record now it's been successfully processed.
				thisStore.scriptMemResults.Delete(0);		
			}
		}
		// PED AND VEHICLE STATS
		else if(thisStore.pedAndVehicleResults.size())
		{
			// Write out a PEDVEHICLESTAT result
			pedAndVehicleStatResults &result = thisStore.pedAndVehicleResults[0];
			MetricCAPTURE_PEDANDVEHICLESTAT m(thisStore.name, thisStore.idx, result);

			// Try to add the metric
			bool success = AppendDebugMetric(&m);
			gnetAssertf(success, "Failed to append Debug telemetry metric '%s'", m.GetMetricName());
		
			if( success )
			{
				USE_DEBUG_MEMORY();
				// Delete this record now it's been successfully processed.
				thisStore.pedAndVehicleResults.Delete(0);		
			}
		}


		// If the store is empty, clean it up
		if( thisStore.IsEmpty() )
		{
			USE_DEBUG_MEMORY();
			m_AutoGPUDataStore.Delete(0);
		}
	}

	return success;
}


// Pass in the count of the max number of packets to write (to allow not consuming the entire buffer), 0 = consume all we can
void CNetworkDebugTelemetry::AppendMetricsAutoGPU(int maxCount)
{
	int packetsWritten = 0;

	// See if we have anything to write
	if(m_AutoGPUDataStore.size())
	{
		bool quitOut = false;

		while(!quitOut)
		{
			if(AppendMetricNextAutoGPUPacket())
			{
				// Count of the number of packets written
				packetsWritten++;
				if( maxCount > 0 )
				{
					if( packetsWritten >= maxCount )
					{
						quitOut = true;
					}
				}
			}
			else
			{
				quitOut = true;
			}
		}
	}
}

#if ENABLE_CUTSCENE_TELEMETRY

// Cutscene Lighting Telemetry
class MetricCUTSCENELIGHTS : public MetricPlayStat
{
	RL_DECLARE_METRIC(CUTSCENELIGHTS, __tel_logchannel, __tel_logleveldevoff);

public:

	explicit MetricCUTSCENELIGHTS(CutsceneLightsTelemetry *pTelemetryData)
	{
		m_CutsceneName = pTelemetryData->m_Name;

		m_DOFWasUsed = pTelemetryData->GetDOFWasActive();
		m_CameraApproved = pTelemetryData->GetCameraApproved();
		m_LightingApproved = pTelemetryData->GetLightingApproved();

		m_DirectionalMin = pTelemetryData->m_DirectionalLightsTimeSample.Min();
		m_DirectionalMax = pTelemetryData->m_DirectionalLightsTimeSample.Max();
		m_DirectionalMean = pTelemetryData->m_DirectionalLightsTimeSample.Mean();
		m_DirectionalStd = pTelemetryData->m_DirectionalLightsTimeSample.StandardDeviation();

		m_SceneMin = pTelemetryData->m_SceneLightsTimeSample.Min();
		m_SceneMax = pTelemetryData->m_SceneLightsTimeSample.Max();
		m_SceneMean = pTelemetryData->m_SceneLightsTimeSample.Mean();
		m_SceneStd = pTelemetryData->m_SceneLightsTimeSample.StandardDeviation(); 

		m_LODMin = pTelemetryData->m_LODLightsTimeSample.Min();
		m_LODMax = pTelemetryData->m_LODLightsTimeSample.Max();
		m_LODMean = pTelemetryData->m_LODLightsTimeSample.Mean();
		m_LODStd = pTelemetryData->m_LODLightsTimeSample.StandardDeviation();

		m_TotalMin = pTelemetryData->m_TotalLightsTimeSample.Min();
		m_TotalMax = pTelemetryData->m_TotalLightsTimeSample.Max();
		m_TotalMean = pTelemetryData->m_TotalLightsTimeSample.Mean();
		m_TotalStd = pTelemetryData->m_TotalLightsTimeSample.StandardDeviation();
	}

	virtual bool Write(RsonWriter* rw) const
	{
		return this->MetricPlayStat::Write(rw)
			// Name
			&& rw->WriteUns("name", m_CutsceneName.GetHash())	
			// DOF Was Active
			&& rw->WriteBool("DOFA", m_DOFWasUsed)
			// Camera Approved
			&& rw->WriteBool("CAMA", m_CameraApproved)
			// Lighting Approved
			&& rw->WriteBool("LGTA", m_LightingApproved)
			// Direction Light Times
			&& rw->Begin("Direct", NULL)
			&& rw->WriteFloat("min", m_DirectionalMin)
			&& rw->WriteFloat("max", m_DirectionalMax)
			&& rw->WriteFloat("avg", m_DirectionalMean)
			&& rw->WriteFloat("std", m_DirectionalStd)
			&& rw->End()
			// Scene Light Times
			&& rw->Begin("Scene", NULL)
			&& rw->WriteFloat("min", m_SceneMin)
			&& rw->WriteFloat("max", m_SceneMax)
			&& rw->WriteFloat("avg", m_SceneMean)
			&& rw->WriteFloat("std", m_SceneStd)
			&& rw->End()
			// LOD Light Times
			&& rw->Begin("LOD", NULL)
			&& rw->WriteFloat("min", m_LODMin)
			&& rw->WriteFloat("max", m_LODMax)
			&& rw->WriteFloat("avg", m_SceneMean)
			&& rw->WriteFloat("std", m_SceneStd)
			&& rw->End()
			// Total Times
			&& rw->Begin("Total", NULL)
			&& rw->WriteFloat("min", m_TotalMin)
			&& rw->WriteFloat("max", m_TotalMax)
			&& rw->WriteFloat("avg", m_TotalMean)
			&& rw->WriteFloat("std", m_TotalStd)
			&& rw->End();
	}

private:

	atHashString	m_CutsceneName;

	bool		m_DOFWasUsed;
	bool		m_CameraApproved;
	bool		m_LightingApproved;

	float		m_DirectionalMin;
	float		m_DirectionalMax;
	float		m_DirectionalMean;
	float		m_DirectionalStd;

	float		m_SceneMin;
	float		m_SceneMax;
	float		m_SceneMean;
	float		m_SceneStd;

	float		m_LODMin;
	float		m_LODMax;
	float		m_LODMean;
	float		m_LODStd;

	float		m_TotalMin;
	float		m_TotalMax;
	float		m_TotalMean;
	float		m_TotalStd;

};

#endif  //ENABLE_CUTSCENE_TELEMETRY

#endif	//ENABLE_STATS_CAPTURE


void CNetworkDebugTelemetry::Init(unsigned initMode)
{
	if (PARAM_nonetwork.Get())
		return;

	if(initMode == INIT_CORE)
	{
		NOTFINAL_ONLY( s_MetricsDebugStartTime = rlGetPosixTime(); )
		NOTFINAL_ONLY( s_MetricsDebugStartTickCount = sysTimer::GetTicks(); )

		m_LastCoordsTime = 0;

		if (PARAM_telemetrycoordsinterval.Get())
		{
			PARAM_telemetrycoordsinterval.Get(m_CoordsInterval);
		}

		if (PARAM_telemetrycoordsdistance.Get())
		{
			PARAM_telemetrycoordsdistance.Get(m_CoordsDelta);
		}

#if ENABLE_STORETRACKING
		m_LastMemUsageTime      = 0;
		m_LastStoreMemDebugTime = 0;
		m_LastDrawListTime      = 0;
#endif // ENABLE_STORETRACKING

#if RAGE_POOL_TRACKING
		m_LastPoolMemDebugTime = 0;
#endif // RAGE_POOL_TRACKING

#if ENABLE_DEBUG_HEAP
		m_LastConsoleMemDebugTime = 0;
#endif // ENABLE_DEBUG_HEAP

#if _METRICS_ENABLE_CLIPS
		if (PARAM_telemetryclipsplayed.Get())
		{
			crClipPlayer::sm_Telemetry = MakeFunctor(CNetworkDebugTelemetry::CollectClipPlayed);
		}

		s_aClipsPlayedData.Reset();
#endif // _METRICS_ENABLE_CLIPS

		s_TimebarMain.clear();
		s_TimebarMain.Reset();

		s_TimebarRender.clear();
		s_TimebarRender.Reset();

		s_TimebarGpu.clear();
		s_TimebarGpu.Reset();

		s_TimebarTimestamp = 0;
		s_TimebarPosX = 0;
		s_TimebarPosY = 0;
	}
	else if(initMode == INIT_SESSION)
	{
	}
}

void CNetworkDebugTelemetry::Shutdown(unsigned shutdownMode)
{
	if (PARAM_nonetwork.Get())
		return;

	if(shutdownMode == SHUTDOWN_CORE)
	{
#if _METRICS_ENABLE_CLIPS
		crClipPlayer::sm_Telemetry = NULL;

		s_aClipsPlayedData.clear();
#endif // _METRICS_ENABLE_CLIPS
	}
	else if(shutdownMode == SHUTDOWN_SESSION)
	{
	}
}

//////////////////////////////////////////////////////////////////////////
//
//
//////////////////////////////////////////////////////////////////////////
void CNetworkDebugTelemetry::UpdateAndFlushDebugMetricBuffer(bool bForce /*= false*/)
{
	rlGamerHandle gamerHandle;
	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	bool bValidCreds = RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex) && rlRos::GetCredentials(localGamerIndex).IsValid() && rlPresence::GetGamerHandle(localGamerIndex, &gamerHandle);
	if(!bValidCreds)
	{
		//NO CREDENTIALS...SEE IF WE'VE REACHED THE THRESHOLD
		//If so, release the memory and flag that we're not taking anymore entries.
		if(m_DebugMetricListSize > NO_CREDS_MAX_BUFFER_SIZE_BYTES)
		{
			gnetDebug1("Releasing Debug Telemetry buffer because it's filled up and we don't have credentials");
			ReleaseDebugMetricBuffer();
			m_bAllowMetricWithNoCreds = false;
		}

		return;
	}

	m_bAllowMetricWithNoCreds = true;  //Our creds are valid, so reset our flag.

	//If we have a critical mass, send them
	if (m_DebugMetricListSize > UPDATE_FLUSH_THESHOLD_BYTES || (bForce && m_DebugMetricListSize > 0))
	{
		gnetDebug1("Submitting Debug Metric buffer with '%d' bytes in metrics", m_DebugMetricListSize);
		const unsigned MEMORY_STORE_METRIC_MEMORY_POLICY_SIZE = 1280; //This needs to be big enough to hold the bigest EXPORTED metric we have ( MetricMEMORY_STORE )
		rlTelemetrySubmissionMemoryPolicy memPolicy(MEMORY_STORE_METRIC_MEMORY_POLICY_SIZE);

		const u64 curTime = rlGetPosixTime();

		//Write the metric to registered streams
		const rlMetric* pMetricIter = m_DebugMetricList.GetFirst();
		while (pMetricIter)
		{
			rlTelemetry::DoWriteToStreams(localGamerIndex, curTime, gamerHandle, *pMetricIter);
			pMetricIter = pMetricIter->GetConstNext();
		}
		
		gnetVerifyf(rlTelemetry::SubmitMetricList(localGamerIndex, &m_DebugMetricList, &memPolicy), "Failed to submit telemetry for MetricMEMORY_STORE");

		memPolicy.ReleaseAndClear();

		ReleaseDebugMetricBuffer();
	}
}

bool CNetworkDebugTelemetry::AppendDebugMetric( const rlMetric* pSrcMetric )
{
	if (!pSrcMetric)
		return false;

	if (PARAM_telemetrynodebug.Get())
		return true;

	if (!rlTelemetry::ShouldSendMetric(*pSrcMetric OUTPUT_ONLY(, "DebugMetric")))
	{
		gnetDebug3( "Ignored metric '%s' - MetricIsDisabled().", pSrcMetric->GetMetricName());
		return true;
	}

	//Check our credentials...however, our credentials come a little later in the game and we don't want loose the initial metrics
	const int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	bool bValidCreds = RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex) && rlRos::GetCredentials(localGamerIndex).IsValid();
	if(!bValidCreds && !m_bAllowMetricWithNoCreds)
	{
		gnetDebug3("Dropping DEBUG metric '%s' because we don't have valid credentials", pSrcMetric->GetMetricName());
		return true;
	}

	const size_t metricSize = pSrcMetric->GetMetricSize();

	rlMetric* pNewMetric = NULL;

#if ENABLE_DEBUG_HEAP
	sysMemAllocator* pDebugAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_DEBUG_VIRTUAL);

	//Allocate a chunk of memory appropriate sized for this metric
	pNewMetric = (rlMetric*) (pDebugAllocator->TryAllocate(metricSize, 0) );
#endif

	if (pNewMetric == NULL)
	{
		gnetDebug3("Failled to allocate memory to add metric '%s' to buffer", pSrcMetric->GetMetricName());
		return false;
	}

	//Slam it, like it's old school
	sysMemCpy((void*)pNewMetric, (void*)pSrcMetric, metricSize);

	//Now put it in the list
    m_DebugMetricList.Append(pNewMetric);
	m_DebugMetricListSize += (unsigned)metricSize;

	gnetDebug3("Appending '%s' of size '%d'", pNewMetric->GetMetricName(), (int)metricSize);

	return true;
}

void CNetworkDebugTelemetry::ReleaseDebugMetricBuffer()
{
	sysMemAllocator* pDebugAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_DEBUG_VIRTUAL);
	if (!gnetVerifyf(pDebugAllocator, "Unable to get the DEBUG allocator to free memory"))
	{
		Quitf("Bailing out because we should NOT be here!");
		return;
	}

    for(rlMetric* m = m_DebugMetricList.Pop(); m; m = m_DebugMetricList.Pop())
    {
		pDebugAllocator->Free(m);
	}
	
	gnetDebug3("ReleaseDebugMetricBuffer freeing '%d'", m_DebugMetricListSize);
	gnetAssert(m_DebugMetricList.GetFirst() == NULL);
	m_DebugMetricListSize = 0;
}

/////////////////////////////////////////////////////////////
// FUNCTION: Update
//    
/////////////////////////////////////////////////////////////

XPARAM(netFunctionProfilerAsserts);
XPARAM(netFunctionProfilerThreshold);

void CNetworkDebugTelemetry::Update()
{
	if (PARAM_nonetwork.Get())
		return;

	//Online bounded operations.
	//No point doing anything if the local player is not Online.
	if (!NetworkInterface::IsLocalPlayerOnline())
		return;

#if NET_PROFILER
	static const unsigned LONG_FRAME_TIME = 100;
	netFunctionProfiler::GetProfiler().SetShouldProfile(true);
	netFunctionProfiler::GetProfiler().StartFrame();
	netFunctionProfiler::GetProfiler().PushProfiler("CNetworkDebugTelemetry::Update"  ASSERT_ONLY(, PARAM_netFunctionProfilerAsserts.Get()));
	unsigned profilerTreshold = 0;
	PARAM_netFunctionProfilerThreshold.Get( profilerTreshold );
	netFunctionProfiler::GetProfiler().SetTimeThreshold( profilerTreshold );
#endif // RSG_OUTPUT && !__PROFILE

	const unsigned curTime = sysTimer::GetSystemMsTime() | 0x01;

	// Every so often send coordinates of player. Should allow us to create density map of all player in the world.
	if(!m_LastCoordsTime || (curTime - m_LastCoordsTime) >= m_CoordsInterval)
	{
		static Vector3 s_LastCoords;
		static bool s_First = false;

		Vector3 delta;
		const Vector3& curCoords = CGameWorld::FindLocalPlayerCoors();

		delta.Subtract(curCoords, s_LastCoords);

		if(s_First || delta.Mag2() >= (m_CoordsDelta*m_CoordsDelta))
		{
#if RAGE_POOL_TRACKING
			if (!s_SendMemoryPoolMetric)
			{
				s_SendMemoryPoolMetric = true;
			}
#endif

#if ENABLE_STORETRACKING
			if (!s_SendMemoryUsageMetric)
			{
				s_SendMemoryStoreMetric = s_SendMemoryUsageMetric = true;
			}

			s_SendDrawListMetric = true;
#endif

			s_LastCoords = curCoords;
			s_First = false;
		}

		m_LastCoordsTime = curTime;
	}

#if ENABLE_DEBUG_HEAP
	static unsigned countConsoleSet = 0;
	if( ((curTime - m_LastConsoleMemDebugTime) >= MEMCONSOLE_INTERVAL || 0 == m_LastConsoleMemDebugTime) && countConsoleSet < 5 )
	{
		NPROFILE( CNetworkDebugTelemetry::AppendMetricConsoleMemory(); )
		m_LastConsoleMemDebugTime = curTime;
		countConsoleSet++;
	}
#endif

#if ENABLE_STORETRACKING
	//--------- Draw Lists
	if (s_SendDrawListMetric && (curTime - m_LastDrawListTime) >= DRAWLIST_INTERVAL)
	{
		NPROFILE( AppendMetricDrawLists(); )
		m_LastDrawListTime = curTime;
		s_SendDrawListMetric = false;
	}

	//--------- Memory metrics
	if(s_SendMemoryStoreMetric && (curTime - m_LastStoreMemDebugTime) >= MEMSTORE_INTERVAL)
	{
		NPROFILE( AppendMetricMemStore(CDebug::GetCurrentMissionName()); )
	}
	else if(s_SendMemoryUsageMetric && (curTime - m_LastMemUsageTime) >= MEMUSAGE_INTERVAL)
	{
		NPROFILE( AppendMetricMemUsage(CDebug::GetCurrentMissionName()); )
	}
#endif // ENABLE_STORETRACKING

#if RAGE_POOL_TRACKING
	if(s_SendMemoryPoolMetric && (curTime - m_LastPoolMemDebugTime) >= POOLUSAGE_INTERVAL)
	{
		NPROFILE( AppendMetricPoolUsage(CDebug::GetCurrentMissionName()); )
		NPROFILE( AppendMetricSkeleton(CDebug::GetCurrentMissionName()); )
	}
#endif

#if ENABLE_CUTSCENE_TELEMETRY
	// Add in cutscene lighting data
	if( g_CutSceneLightTelemetryCollector.ShouldOutputTelemetry() )
	{
		NPROFILE( AppendMetricCutsceneLights( ); )
	}
#endif // ENABLE_CUTSCENE_TELEMETRY

#if ENABLE_STATS_CAPTURE
	// At the end, because it'll consume all the remaining Telemetry buffer if there's enough data to write
	NPROFILE( AppendMetricsAutoGPU(0); )
#endif // ENABLE_STATS_CAPTURE

	NPROFILE( AppendMetricTimebars(); )

	NPROFILE( UpdateAndFlushDebugMetricBuffer(); )

#if NET_PROFILER
	netFunctionProfiler::GetProfiler().PopProfiler("CNetworkDebugTelemetry::Update", LONG_FRAME_TIME);
	netFunctionProfiler::GetProfiler().TerminateFrame();
#endif // NET_PROFILER
}

void
CNetworkDebugTelemetry::TelemetryConfigChanged()
{
	// Checking it each frame is a bit slow, so we do it each time the config changes.
	// This assumes MetricTIMEBARS is NOT sampled per metric call
	MetricTIMEBARS m;
	m_SendTimeBars = !m.IsDisabled();
}

void
CNetworkDebugTelemetry::SetCoordsInterval(const unsigned intervalMs)
{
	if(AssertVerify(intervalMs >= MINIMUM_COORDS_INTERVAL))
	{
		m_CoordsInterval = intervalMs;
	}
}

unsigned
CNetworkDebugTelemetry::GetCoordsInterval()
{
	return m_CoordsInterval;
}

void
CNetworkDebugTelemetry::SetCoordsDelta(const float delta)
{
	if(AssertVerify(delta >= MINIMUM_COORDS_DELTA))
	{
		m_CoordsDelta = delta;
	}
}

float
CNetworkDebugTelemetry::GetCoordsDelta()
{
	return m_CoordsDelta;
}

void 
CNetworkDebugTelemetry::ResetMetricsDebugStartTime()
{
	if (!PARAM_nonetlogs.Get())
	{
		s_MetricsDebugStartTime = rlGetPosixTime();
		s_MetricsDebugStartTickCount = sysTimer::GetTicks();
	}
}

#if RAGE_POOL_TRACKING

void CNetworkDebugTelemetry::AppendMetricPoolUsage(const char* missionName, bool debug /*= false*/)
{
	gnetAssert(missionName);

	MetricMEMORY_POOL m(missionName);

	if (debug)
		Displayf("Pool, Hash, StorageSize, Size, Peak");

	atArray<PoolTracker*>& trackers = PoolTracker::GetTrackers();
	for (int i = 0; i < trackers.GetCount(); ++i)
	{
		const PoolTracker* pTracker = trackers[i];		

		if (gnetVerify(pTracker) && gnetVerify(pTracker->GetName()))
		{
			if (debug)
				Displayf("%s, 0x%X, %" SIZETFMT "d, %d, %d", pTracker->GetName(), atStringHash(pTracker->GetName()), pTracker->GetStorageSize(), pTracker->GetSize(), pTracker->GetPeakSlotsUsed());

			MetricMEMORY_POOL::sMemPoolInfo info;
			info.SetInfo(pTracker->GetName(), pTracker->GetPeakSlotsUsed());
			m.Add(info);

			if (m.IsFull())
			{
				gnetVerifyf(AppendDebugMetric(&m), "Failed to append Debug telemetry metric '%s'", m.GetMetricName());
				m.Clear();
			}
		}
	}

	if (!m.IsEmpty())
	{
		gnetVerifyf(AppendDebugMetric(&m), "Failed to append Debug telemetry metric '%s'", m.GetMetricName());
	}

	m_LastPoolMemDebugTime = sysTimer::GetSystemMsTime();
	s_SendMemoryPoolMetric = false;
};
#endif // RAGE_POOL_TRACKING

#if ENABLE_STORETRACKING
void CNetworkDebugTelemetry::AppendMetricMemUsage(const char* missionName, bool debug /*= false*/)
{
	gnetAssert(missionName);

	s_SendMemoryUsageMetric = false;
	m_LastMemUsageTime      = sysTimer::GetSystemMsTime() | 0x01;

	MetricMEMORY_USAGE m(missionName, debug);
	gnetVerifyf(AppendDebugMetric(&m), "Failed to append Debug telemetry metric '%s'", m.GetMetricName());
}

void CNetworkDebugTelemetry::AppendMetricMemStore(const char* missionName, bool debug /*= false*/)
{
	gnetAssert(missionName);

	s_SendMemoryStoreMetric = false;
	m_LastStoreMemDebugTime = sysTimer::GetSystemMsTime() | 0x01;

	MetricMEMORY_STORE m(missionName, debug);
	gnetVerifyf(AppendDebugMetric(&m), "Failed to append Debug telemetry metric '%s'", m.GetMetricName());
	
}
#endif // ENABLE_STORETRACKING


void
CNetworkDebugTelemetry::AppendMetricDrawLists( )
{
#if !RSG_FINAL
	MetricDRAW_LISTS m;
	if(!m.IsDisabled())
	{
		NPROFILE( gnetVerifyf(AppendDebugMetric(&m), "Failed to append Debug telemetry metric '%s'", m.GetMetricName()); )
	}
	else
	{
		MetricGAMEPLAY_FPS m; 
		gnetVerifyf(AppendDebugMetric(&m), "Failed to append Debug telemetry metric '%s'", m.GetMetricName());
	}
#endif
}

void
CNetworkDebugTelemetry::AppendMetricCutsceneLights( )
{
#if ENABLE_CUTSCENE_TELEMETRY
	//Instantiate the metric.
	MetricCUTSCENELIGHTS m(&g_CutSceneLightTelemetryCollector);
	//Append the metric.
	gnetVerifyf(AppendDebugMetric(&m), "Failed to append Debug telemetry metric '%s'", m.GetMetricName());
#endif
}

void 
CNetworkDebugTelemetry::AppendMetricTimebars( )
{
	if(m_SendTimeBars && (s_TimebarMain.GetCount() > 0 || s_TimebarRender.GetCount() > 0 || s_TimebarGpu.GetCount() > 0))
	{
		unsigned timebar = 0;

		atArray < sMetricInfoTimebar >*  timebarInfo = 0;

		if (s_TimebarMain.GetCount() > 0)
		{
			timebarInfo = &s_TimebarMain;
			timebar     = TIMEBAR_MAIN;
		}
		else if (s_TimebarRender.GetCount() > 0)
		{
			timebarInfo = &s_TimebarRender;
			timebar     = TIMEBAR_RENDER;
		}
		else if (s_TimebarGpu.GetCount() > 0)
		{
			timebarInfo = &s_TimebarGpu;
			timebar     = TIMEBAR_GPU;
		}

		if (gnetVerify(timebarInfo))
		{
			sMetricInfoTimebar* info = timebarInfo->GetElements();

			MetricTIMEBARS m(s_TimebarTimestamp, timebar, info[0].m_Name, info[0].m_Time, s_TimebarPosX, s_TimebarPosY);
			if(gnetVerifyf(AppendDebugMetric(&m), "Failed to append Debug telemetry metric '%s'", m.GetMetricName()))
			{
				timebarInfo->Delete(0);
			}
		}
	}
	else
	{
		//Make sure we cleanup in case the log level was changed.
		if(s_TimebarTimestamp != 0)
		{
			s_TimebarTimestamp = 0;
			s_TimebarPosX = 0;
			s_TimebarPosY = 0;
		}

		if (s_TimebarMain.GetCount() > 0)
		{
			s_TimebarMain.clear();
			s_TimebarMain.Reset();
		}

		if (s_TimebarRender.GetCount() > 0)
		{
			s_TimebarRender.clear();
			s_TimebarRender.Reset();
		}

		if (s_TimebarGpu.GetCount() > 0)
		{
			s_TimebarGpu.clear();
			s_TimebarGpu.Reset();
		}
	}
}

void CNetworkDebugTelemetry::AppendMetricsMissionStarted(const char* missionName)
{
	gnetAssert(missionName);

	AppendMetricSkeleton(missionName);

#if ENABLE_STORETRACKING
	AppendMetricMemStore(missionName);
#endif

#if RAGE_POOL_TRACKING	
	AppendMetricPoolUsage(missionName);
#endif
}

void CNetworkDebugTelemetry::AppendMetricsMissionOver(const char* missionName)
{
	gnetAssert(missionName);

	AppendMetricSkeleton(missionName);

#if ENABLE_STORETRACKING
	AppendMetricMemStore(missionName);
#endif

#if RAGE_POOL_TRACKING	
	AppendMetricPoolUsage(missionName);
#endif

#if _METRICS_ENABLE_CLIPS
	//Lets flush some metrics in CLIPSPLAYED_MISSIONENDED_INTERVAL.
	s_SendClipsPlayedMetric = sysTimer::GetSystemMsTime() - (CLIPSPLAYED_INTERVAL - CLIPSPLAYED_MISSIONENDED_INTERVAL);
#endif

	UpdateAndFlushDebugMetricBuffer(true);
}

void CNetworkDebugTelemetry::AppendMetricConsoleMemory()
{
	//Instantiate the metric.
	MetricCONSOLE_MEMORY m;
	//Append the metric.
	gnetVerifyf(AppendDebugMetric(&m), "Failed to append Debug telemetry metric '%s'", m.GetMetricName());
}

void CNetworkDebugTelemetry::AppendMetricSkeleton(const char* missionName)
{
	MetricMEMORY_SKELETON m(missionName);
	gnetVerifyf(AppendDebugMetric(&m), "Failed to append Debug telemetry metric '%s'", m.GetMetricName());
}

#if RAGE_TIMEBARS
void CNetworkDebugTelemetry::CollectMetricTimebars()
{
	if(m_SendTimeBars)
	{
		gnetAssert(s_TimebarMain.GetCount() <= 0);
		gnetAssert(s_TimebarRender.GetCount() <= 0);
		gnetAssert(s_TimebarRender.GetCount() <= 0);

		if (s_TimebarMain.GetCount() <= 0 && s_TimebarRender.GetCount() <= 0 && s_TimebarRender.GetCount() <= 0)
		{
			//------------ MAIN ------------
			s_TimebarMain.clear();
			s_TimebarMain.Reset();

			const pfTimeBars&                 timebarMain = g_pfTB.GetTimeBar(TIMEBAR_MAIN);
			const pfTimeBars::sTimebarFrame&  frameMain   = timebarMain.GetRenderTimebarFrame();

			for (int j=0; j<frameMain.m_number; j++)
			{
				const pfTimeBars::sFuncTime& funcTime = frameMain.m_pTimes[j];

				//Ignore all detail bars.
				if (funcTime.detail)
					continue;

				if(funcTime.nameptr[0] != '\0')
				{
					sMetricInfoTimebar info;
					info.m_Name = atStringHash(funcTime.nameptr);
					info.m_Time = funcTime.endTime - funcTime.startTime;

					s_TimebarMain.PushAndGrow(info);
				}
			}

			//------------ RENDER ------------
			s_TimebarRender.clear();
			s_TimebarRender.Reset();

			const pfTimeBars&                timebarRender = g_pfTB.GetTimeBar(TIMEBAR_RENDER);
			const pfTimeBars::sTimebarFrame& frameRender   = timebarRender.GetRenderTimebarFrame();

			for (int j=0; j<frameRender.m_number; j++)
			{
				const pfTimeBars::sFuncTime& funcTime = frameRender.m_pTimes[j];

				//Ignore all detail bars.
				if (funcTime.detail)
					continue;

				if(funcTime.nameptr[0] != '\0')
				{
					sMetricInfoTimebar info;
					info.m_Name = atStringHash(funcTime.nameptr);
					info.m_Time = funcTime.endTime - funcTime.startTime;

					s_TimebarRender.PushAndGrow(info);
				}
			}

			//------------ GPU ------------
			s_TimebarGpu.clear();
			s_TimebarGpu.Reset();

			const pfTimeBars&                 timebarGpu  = g_pfTB.GetTimeBar(TIMEBAR_GPU);
			const pfTimeBars::sTimebarFrame&  frameGpu    = timebarGpu.GetRenderTimebarFrame();

			for (int j=0; j<frameGpu.m_number; j++)
			{
				const pfTimeBars::sFuncTime& funcTime = frameGpu.m_pTimes[j];

				//Ignore all detail bars.
				if (funcTime.detail)
					continue;

				if(funcTime.nameptr[0] != '\0')
				{
					sMetricInfoTimebar info;
					info.m_Name = atStringHash(funcTime.nameptr);
					info.m_Time = funcTime.endTime - funcTime.startTime;

					s_TimebarGpu.PushAndGrow(info);
				}
			}

			s_TimebarTimestamp = rlGetPosixTime();

			const Vector3 coords = CGameWorld::FindLocalPlayerCoors();
			s_TimebarPosX = (int) coords.x;
			s_TimebarPosY = (int) coords.y;
		}
	}
}
#endif // RAGE_TIMEBARS

#if __BANK
// Heatmap metric (B*776302)
class MetricHEATMAP : public MetricPlayerCoords
{
	RL_DECLARE_METRIC(HEATMAP, TELEMETRY_CHANNEL_DEV_CAPTURE, __tel_logleveldevoff);

public:

	explicit MetricHEATMAP(u32 mainShortfall, u32 vramShortfall)
	{
		m_MainShortfall = mainShortfall;
		m_VramShortfall = vramShortfall;
	}

	virtual bool Write(RsonWriter* rw) const
	{
		return this->MetricPlayerCoords::Write(rw)
			&& rw->WriteUns("msf", m_MainShortfall)
			&& rw->WriteUns("vsf", m_VramShortfall);
	}

private:
	u32 m_MainShortfall;
	u32 m_VramShortfall;
};



void	CNetworkDebugTelemetry::AppendMetricHeatMap( u32 mainShortfall, u32 vramShortfall )
{
	//Instantiate the metric.
	MetricHEATMAP m(mainShortfall, vramShortfall);

	//Append the metric.
	bool bSent = gnetVerifyf(AppendDebugMetric(&m), "Failed to append Debug telemetry metric '%s'", m.GetMetricName());
		
	if( PARAM_addTelemetryDebugTTY.Get() )
	{
		static int	sSentHeatmapWrites = 0;
		static int	sFailedHeatmapWrites = 0;

		if(bSent)
		{
			sSentHeatmapWrites++;

			Vector3 position = m.GetCoords();
			Displayf("MetricHEATMAP: Written. Pos (%f,%f,%f): Success == %d : Failed == %d", position.x, position.y, position.z, sSentHeatmapWrites, sFailedHeatmapWrites );
		}
		else
		{
			sFailedHeatmapWrites++;
		}
	}
}

#endif	//__BANK

#if _METRICS_ENABLE_CLIPS

class MetricCLIP : public MetricPlayStat
{
	RL_DECLARE_METRIC(CLIP, __tel_logchannel, __tel_logleveldevoff);

public:
	MetricCLIP(const u32 clipNameHash, const u32 dictionaryNameHash, const u32 timesPlayed) 
		: m_clipNameHash(clipNameHash)
		, m_dictionaryNameHash(dictionaryNameHash)
		, m_timesPlayed(timesPlayed)
	{
	}

	virtual bool Write(RsonWriter* rw) const
	{
		return this->MetricPlayStat::Write(rw) 
			&& rw->WriteUns("cn", m_clipNameHash)
			&& rw->WriteUns("dn", m_dictionaryNameHash)
			&& rw->WriteUns("tp", m_timesPlayed);
	}

private:
	u32  m_clipNameHash;
	u32  m_dictionaryNameHash;
	u32  m_timesPlayed;
};

void  CNetworkDebugTelemetry::AppendMetricsClipsPlayed( )
{
	bool done = false;

	int numClipsSent = 0;

	for (int i=s_aClipsPlayedData.GetCount()-1; i>=0 && !done; i--)
	{
		gnetAssert( i < s_aClipsPlayedData.GetCount() );

		MetricCLIP m(s_aClipsPlayedData[i].m_ClipName, s_aClipsPlayedData[i].m_DictionaryName, s_aClipsPlayedData[i].m_TimesPlayed);

		bool result = AppendDebugMetric(&m);
		gnetAssertf(result, "Failed to append Debug telemetry metric '%s'", m.GetMetricName());

		if(result)
		{
			s_aClipsPlayedData[i].m_KeyName        = 0;
			s_aClipsPlayedData[i].m_ClipName       = 0;
			s_aClipsPlayedData[i].m_DictionaryName = 0;
			s_aClipsPlayedData[i].m_TimesPlayed    = 0;
			numClipsSent++;
		}
		else
		{
			done = true;
			break;
		}
	}

	gnetDebug1("Managed to append \"%d\" metrics out of \"%d\"", numClipsSent, s_aClipsPlayedData.GetCount());
	gnetAssertf(numClipsSent>0, "Did not manage to send any metrics.");

	//We are done flushing all clips played so lets wait again.
	s_SendClipsPlayedMetric = sysTimer::GetSystemMsTime() | 0x01;

	//Lets make it flush fast unless we managed to flush all!!!
	if (numClipsSent < s_aClipsPlayedData.GetCount())
	{
		s_SendClipsPlayedMetric -= (CLIPSPLAYED_INTERVAL - CLIPSPLAYED_MISSIONENDED_INTERVAL);
	}

	//Resize
	s_aClipsPlayedData.Resize( s_aClipsPlayedData.GetCount() - numClipsSent );
}

void CNetworkDebugTelemetry::CollectClipPlayed(const crClip* clip)
{
	if (!PARAM_telemetryclipsplayed.Get())
		return;

	if (!gnetVerify(clip))
		return;

	//If we can't get the lock now lets try next time.
	if (!sm_ClipsPlayedCriticalSection.TryLock())
	{
		gnetDebug3("Critical Section issue - Discarded keyName=%s", clip->GetName());
		return;
	}

	const unsigned curTime = sysTimer::GetSystemMsTime() | 0x01;
	if (s_aClipsPlayedData.GetCount() >= MAX_NUMBER_CLIPS || (curTime - s_SendClipsPlayedMetric) >= CLIPSPLAYED_INTERVAL)
	{
		AppendMetricsClipsPlayed();
	}

	const char *szClipDictionaryName = NULL;

	char keyName[128];

	atString clipName(clip->GetName());
	if(clipName.StartsWith("pack:/"))
	{
		clipName.Replace("pack:/", "");
		clipName.Replace(".clip", "");

		for(int clipDictionaryIndex = 0; clipDictionaryIndex < g_ClipDictionaryStore.GetCount(); clipDictionaryIndex ++)
		{
			if(g_ClipDictionaryStore.IsValidSlot(strLocalIndex(clipDictionaryIndex)))
			{
				crClipDictionary *pClipDictionary = g_ClipDictionaryStore.Get(strLocalIndex(clipDictionaryIndex));
				if(pClipDictionary == clip->GetDictionary())
				{
					szClipDictionaryName = g_ClipDictionaryStore.GetName(strLocalIndex(clipDictionaryIndex));
					break;
				}
			}
		}

		if(szClipDictionaryName)
		{
			formatf(keyName, "%s/%s", szClipDictionaryName, clipName.c_str());
		}
		else
		{
			formatf(keyName, "%s", clipName.c_str());
		}
	}
	else
	{
		formatf(keyName, "%s", clip->GetName());
	}

	sMetricClipInfo newClip;
	newClip.m_KeyName        = atStringHash(keyName);
	newClip.m_ClipName       = atStringHash(clipName.c_str());
	newClip.m_DictionaryName = szClipDictionaryName ? atStringHash(szClipDictionaryName) : 0;

	sMetricClipInfo* start     = s_aClipsPlayedData.begin();
	sMetricClipInfo* end       = s_aClipsPlayedData.end();
	sMetricClipInfo* clipWhere = std::lower_bound(start, end, newClip);

	if (clipWhere && clipWhere->m_KeyName == newClip.m_KeyName)
	{
		if (clipWhere->m_TimesPlayed >= 0x7FFFFFFF)
		{
			MetricCLIP m(clipWhere->m_ClipName, clipWhere->m_DictionaryName, clipWhere->m_TimesPlayed);

			gnetVerifyf(AppendDebugMetric(&m), "Failed to append Debug telemetry metric '%s'", m.GetMetricName());

			clipWhere->m_TimesPlayed = 0;
		}

		clipWhere->m_TimesPlayed += 1;
		return;
	}

	if (s_aClipsPlayedData.GetCount() >= MAX_NUMBER_CLIPS)
	{
		gnetError("Discarded keyName=%s < clipName=%s, DictionaryName=%s >", keyName, clipName.c_str(), szClipDictionaryName?szClipDictionaryName:"");
		gnetAssertf(0, "Discarded clipName %s", clip->GetName());
		return;
	}

	gnetDebug1("Add new clipName=\"%s : %d\" < clipName=%s < %d >, DictionaryName=%s < %d > >", keyName, newClip.m_KeyName, clipName.c_str(), newClip.m_ClipName, szClipDictionaryName?szClipDictionaryName:"", newClip.m_DictionaryName);

	const int index = static_cast<int>(clipWhere - start);
	s_aClipsPlayedData.Insert(index);
	s_aClipsPlayedData[index].m_KeyName        = newClip.m_KeyName;
	s_aClipsPlayedData[index].m_ClipName       = newClip.m_ClipName;
	s_aClipsPlayedData[index].m_DictionaryName = newClip.m_DictionaryName;
	s_aClipsPlayedData[index].m_TimesPlayed = 1;

	//Critical section
	sm_ClipsPlayedCriticalSection.Unlock();
}
#endif // _METRICS_ENABLE_CLIPS


#if ENABLE_STATS_CAPTURE

class MetricSHAPETEST_COST : public MetricPlayerCoords
{
	RL_DECLARE_METRIC(SHAPETEST_COST, __tel_logchannel, __tel_logleveldevoff);

public:

	explicit MetricSHAPETEST_COST(float min, float max, float avg)
	{
		m_min = min;
		m_max = max;
		m_avg = avg;
	}

	virtual bool Write(RsonWriter* rw) const
	{
		return this->MetricPlayerCoords::Write(rw)
			&& rw->WriteFloat("min", m_min)
			&& rw->WriteFloat("max", m_max)
			&& rw->WriteFloat("avg", m_avg);
	}

private:
	float m_min;
	float m_max;
	float m_avg;
};


void	CNetworkDebugTelemetry::AppendMetricShapetestCost(float min, float max, float average)
{
	//Instantiate the metric.
	MetricSHAPETEST_COST m(min, max, average);

	// Try to add the metric
	bool success = AppendDebugMetric(&m);
	gnetAssertf(success, "Failed to append Debug telemetry metric '%s'", m.GetMetricName());

	if( PARAM_addTelemetryDebugTTY.Get() )
	{
		static int	sSentShapetestWrites = 0;
		static int	sFailedShapetestWrites = 0;

		if(success)
		{
			sSentShapetestWrites++;
			Vector3 position = m.GetCoords();
			Displayf("MetricSHAPETEST_COST: Written. Pos (%f,%f,%f): Success == %d : Failed == %d", position.x, position.y, position.z, sSentShapetestWrites, sFailedShapetestWrites );
		}
		else
		{
			sFailedShapetestWrites++;
		}
	}

}

#endif	//ENABLE_STATS_CAPTURE


class MetricAUD_WATERMARK : public MetricPlayerCoords
{
	RL_DECLARE_METRIC(AUD_WATERMARK, __tel_logchannel, __tel_loglevel);
private:
	int m_watermark;
	int m_char;
	int m_mission;

public:
	MetricAUD_WATERMARK (int watermark) 
		: m_watermark(watermark)
		, m_char(StatsInterface::GetStatsModelPrefix())
		, m_mission(0)
	{
		if (CTheScripts::GetPlayerIsOnAMission())
		{
			m_mission = atStringHash(CDebug::GetCurrentMissionName());
		}
	}

	virtual bool Write(RsonWriter* rw) const
	{
		return this->MetricPlayerCoords::Write(rw) 
			&& rw->WriteInt("watermark", m_watermark)
			&& rw->WriteInt("char", m_char)
			&& rw->WriteInt("mission", m_mission);
	}
};

void  CNetworkDebugTelemetry::AppendMetricsAudioPoolHighWatermark(const int watermark)
{
	const u32 AUD_WATERMARK_INTERVAL = 20000;

	static u32 s_lastTimeInterval = 0;

	const u32 curTime = sysTimer::GetSystemMsTime();

	if (!s_lastTimeInterval || s_lastTimeInterval+AUD_WATERMARK_INTERVAL<curTime)
	{
		s_lastTimeInterval = curTime;

		static int s_lastWaterMark = 0;
		if(s_lastWaterMark != watermark)
		{
			s_lastWaterMark = watermark;
			MetricAUD_WATERMARK m(watermark);
			ASSERT_ONLY(bool success = )AppendDebugMetric(&m);
			ASSERT_ONLY(gnetAssertf(success, "Failed to append Debug telemetry metric '%s'", m.GetMetricName());)
		}
	}
}

#endif // RSG_OUTPUT
