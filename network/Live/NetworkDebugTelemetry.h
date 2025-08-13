//
// NetworkDebugTelemetry.h
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef _NETWORKDEBUGTELEMETRY_H_
#define _NETWORKDEBUGTELEMETRY_H_

#if RSG_OUTPUT

#include "fwtl/assetstore.h"
#include "profile/timebars.h"
#include "rline/rltelemetry.h"
#include "system/AutoGPUCapture.h"
#include "cranimation/animation_config.h"

#define _METRICS_ENABLE_CLIPS (CR_DEV)

namespace rage
{
	class crClip;
}

// PURPOSE
//   This class is responsible for appending debug metrics.
class CNetworkDebugTelemetry
{
public:
	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);
	static void Update();
	static void TelemetryConfigChanged();

	static void UpdateAndFlushDebugMetricBuffer(bool bForce = false);

	//Append debug metric.
	static bool AppendDebugMetric(const rlMetric* pMetric);

	//Sets the interval in milliseconds between recording player coords.
	static void SetCoordsInterval(const unsigned intervalMs);
	static unsigned GetCoordsInterval();

	//Sets the minimum delta between player positions before player
	//coordinates will be recorded.
	static void SetCoordsDelta(const float delta);
	static float GetCoordsDelta();

	//Reset values for the metrics precison Time in microseconds.
	static void  ResetMetricsDebugStartTime();

	//Dev-only telemetry request: audio environment group pool utilization.
	static void  AppendMetricsAudioPoolHighWatermark(const int watermark);

	//Sets the mission debug related metrics.
	static void  AppendMetricsMissionStarted(const char* missionName);
	static void  AppendMetricsMissionOver(const char* missionName);

public:
	static void  AppendMetricSkeleton(const char* missionName);
#if __BANK
	static void	 AppendMetricHeatMap(u32 mainShortfall, u32 vramShortfall);
#endif
#if ENABLE_STATS_CAPTURE
	static void	AppendMetricShapetestCost(float min, float max, float average);
#endif
#if ENABLE_STORETRACKING
	static void  AppendMetricMemUsage(const char* missionName, bool debug = false);	
	static void  AppendMetricMemStore(const char* missionName, bool debug = false);	
#endif
#if RAGE_POOL_TRACKING
	static void  AppendMetricPoolUsage(const char* missionName, bool debug = false);
#endif

#if RAGE_TIMEBARS
	static void  CollectMetricTimebars();
#endif
#if _METRICS_ENABLE_CLIPS
	static void  CollectClipPlayed(const crClip* clipName);
#endif

#if ENABLE_STATS_CAPTURE
public:
	class AutoGPUCaptureTelemetryStore
	{
		// NOTE: remember to keep this list & the function IsEmpty(), 
		//  aswell as the function SendNextAutoGPUPacket(), up to date with metricsList (AutoGPUCapture.h)
	public:
		int idx;
		atString name;													// The name of the zone
		atArray<framesPerSecondResult> fpsResults;
		atArray<msPerFrameResult> msPerFrameResults;
		atArray<gpuTimingResult> gpuResults;
		atArray<cpuTimingResult> cpuResults;
		atArray<threadTimingResult> threadResults;
		atArray<memoryBucketUsageResult> memoryResults;
		atArray<streamingMemoryResult> streamingMemoryResults;
		atArray<drawListResult> drawListResults;
		atArray<memoryDistributionResult> memDistributionResults;
		atArray<scriptMemoryUsageResult> scriptMemResults;
		atArray<pedAndVehicleStatResults> pedAndVehicleResults;

		AutoGPUCaptureTelemetryStore& operator =(const metricsList &other);

		bool IsEmpty();
	};

	static atArray<AutoGPUCaptureTelemetryStore> m_AutoGPUDataStore;
	static void AddAutoGPUTelemetry( metricsList *pList );

private:
	static bool AppendMetricNextAutoGPUPacket();
	static void AppendMetricsAutoGPU(int maxCount);
#endif	//ENABLE_STATS_CAPTURE

private:
	static void  AppendMetricDrawLists( );
	static void  AppendMetricCutsceneLights( );
	static void  AppendMetricTimebars();
	static void  AppendMetricConsoleMemory();
#if _METRICS_ENABLE_CLIPS
	static void  AppendMetricsClipsPlayed( );
#endif

private:
#if RAGE_POOL_TRACKING
	static unsigned   m_LastPoolMemDebugTime; //Last time store memory usage stats were set.
#endif
	//Player coords
	static unsigned    m_CoordsInterval;
	static float       m_CoordsDelta;
	static unsigned    m_LastCoordsTime;

#if ENABLE_STORETRACKING
	static unsigned     m_LastMemUsageTime;      //Last time memory usage stats were set.
	static unsigned     m_LastStoreMemDebugTime; //Last time store memory usage stats were set.
	static unsigned     m_LastDrawListTime;      //Last Draw List stats where sent.
#endif // ENABLE_STORETRACKING

#if ENABLE_DEBUG_HEAP
	static unsigned     m_LastConsoleMemDebugTime;  //Last time console memory size stats were set.
#endif // ENABLE_DEBUG_HEAP

	static const unsigned UPDATE_FLUSH_THESHOLD_BYTES = 1024;
	static const unsigned NO_CREDS_MAX_BUFFER_SIZE_BYTES = 4096;

	static void ReleaseDebugMetricBuffer();
	static rlMetricList m_DebugMetricList; //list of rlMetrics
	static unsigned m_DebugMetricListSize; //Keep track of how big the list is in bytes
	static bool m_bAllowMetricWithNoCreds;			//Switch to manage when we're allowing metrics to be appended whilst no creds.
	static bool m_SendTimeBars;
};

#endif // !__FINAL

#endif // _NETWORKDEBUGTELEMETRY_H_


