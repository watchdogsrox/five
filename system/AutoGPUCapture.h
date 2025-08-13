#ifndef INC_AUTO_GPU_CAPTURE_H_
#define INC_AUTO_GPU_CAPTURE_H_

#include "debug/debug.h"
#include "scene/datafilemgr.h"
#include "math/math.h"
#include "script/gta_thread.h"

class debugLocationList;

#define ENABLE_STATS_CAPTURE	(!__FINAL)

#if ENABLE_STATS_CAPTURE
	#define STATS_CAPTURE_ONLY(_x)	_x
#else
	#define STATS_CAPTURE_ONLY(_x)
#endif // ENABLE_STATS_CAPTURE




////////////////////////////////////////////////////////////////////////////////////////////////////

class framesPerSecondResult
{
public:
	float min;
	float max;
	float average;
	float std;

	PAR_SIMPLE_PARSABLE;
};

namespace MetricsCapture
{
	template<typename T> class Limits
	{
	public:
		static T Min();
		static T Max();
	};

	template<> class Limits<float>
	{
	public:
		static float Min() { return FLT_MIN; }
		static float Max() { return FLT_MAX; }
	};

	template<> class Limits<u32>
	{
	public:
		static u32 Min() { return 0; }
		static u32 Max() { return 0xFFFFFFFF; }
	};



	template<typename T> class SampledValue
	{
	public:
		SampledValue()
		{
			Reset();
		}

		void AddSample(T sample)
		{
			++m_Samples;

			// From http://www.johndcook.com/standard_deviation.html 
			// See Knuth TAOCP vol 2, 3rd edition, page 232
			if (m_Samples == 1)
			{
				m_Mean = float(sample);
				m_Sum = 0.0f;
			}
			else
			{
				float oldMean = m_Mean;
				float oldSum = m_Sum;
				m_Mean = oldMean + (sample - oldMean)/m_Samples;
				m_Sum = oldSum + (sample - oldMean)*(sample - m_Mean);
			}

			m_Min = rage::Min(sample, m_Min);
			m_Max = rage::Max(sample, m_Max);
		}

		void HalveSamples()
		{
			int oldSamples = m_Samples;
			m_Samples /= 2;
			m_Sum = (m_Sum / oldSamples) * m_Samples;
		}

		void Reset()
		{
			m_Sum = 0;
			m_Mean = 0;
			m_Samples = 0;
			m_Min = Limits<T>::Max();
			m_Max = Limits<T>::Min();
		}

		const T& Min() const
		{
			return m_Min;
		}

		const T& Max() const
		{
			return m_Max;
		}

		T Mean() const
		{
			return static_cast<T>(m_Samples > 0 ? m_Mean : 0.0f);
		}

		float Variance() const
		{
			return ( (m_Samples > 1) ? m_Sum/(static_cast<float>(m_Samples) - 1) : 0.0f );
		}

		float StandardDeviation() const
		{
			return sqrt( Variance() );
		}

		int Samples() const
		{
			return m_Samples;
		}

		T Sum() const
		{
			return static_cast<T>(m_Mean * m_Samples);	
		}

	private:
		int m_Samples;
		float m_Mean;
		float m_Sum;
		T m_Min;
		T m_Max;
	};

#if ENABLE_STATS_CAPTURE

	extern debugLocationList *g_LocationList;
	inline void SetLocationList(debugLocationList* list) { g_LocationList = list; }
	
#if __BANK
	void AddWidgets(bkBank& bank);
#endif

	void Init();
	void Update(float time);
	void Shutdown();

	void DrawWindows(); // render thread

	void SmokeTestSectionCaptureStart(const char * section);
	void SmokeTestSectionCaptureStop();
	void SmokeTestSaveResults(const char* report);
	
	void MetricsZonesClear();
	void MetricsZonesDisplay(bool show);
	void MetricsZonesSave();
	void MetricsZonesSaveToFilename(const char *pFilename);

	void MetricsZoneStart(const char * zoneName);
	void MetricsZoneStop();
	void MetricsZoneWriteTelemetry();

	void MetricsSmokeTestStart(const char * section);
	void MetricsSmokeTestStop();
	void MetricsSmokeTestSave();
	void MetricsSmokeTestSaveToFilename(const char *pFilename);
	void MetricsSmokeTestClear();
	void MetricsSmokeTestDisplay(bool show);

	void SetScriptReportAllScriptTimes();
	void SetScriptReport( const char *pScriptName );
	void ScriptTimingUpdate(float currentTime);
	float GetScriptUpdateTime(scrThreadId scriptThreadID);
	bool GetScriptPeakAverageTime(scrThreadId scriptThreadID, float &peak, float &average);
};

///////////////////////////////////////////////////////////////////////////////////////////////////

class msPerFrameResult
{
public:
	float min;
	float max;
	float average;
	float std;
	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
class gpuTimingResult
{
public:
	int idx;
	atString name;
	float time;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
class cpuTimingResult
{
public:
	int idx;
	atString name;
	atString set;
	float min;
	float max;
	float average;
	float std;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
class threadTimingResult
{
public:
	int idx;
	atString name;
	float min;
	float max;
	float average;
	float std;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
class memoryHeapResult
{
public:
	int idx;
	atString name;
	size_t	used;
	size_t	free;
	size_t	total;
	size_t	peak;
	size_t	fragmentation;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
class memoryUsageResult
{
public:
	int min;
	int max;
	int average;
	float std;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
class memoryBucketUsageResult
{
public:
	int idx;
	atString name;
	memoryUsageResult gameVirtual;
	memoryUsageResult gamePhysical;
	memoryUsageResult resourceVirtual;
	memoryUsageResult resourcePhysical;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class scriptMemoryUsageResult
{
public:
	memoryUsageResult physicalMem;
	memoryUsageResult virtualMem;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
class drawListResult
{
public:
	int idx;
	atString name;
	int min;
	int max;
	int average;
	float std;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
class streamingStatsResult
{
public:
	atString name;

	u32 avgLoaded;
	u32 minLoaded;
	u32 maxLoaded;
	float stdLoaded;
	u32 totalLoaded;

	u32 avgRequested;
	u32 minRequested;
	u32 maxRequested;
	float stdRequested;
	u32 totalRequested;

	PAR_SIMPLE_PARSABLE;
};


////////////////////////////////////////////////////////////////////////////////////////////////////
class streamingMemoryCategoryResult
{
public:
	atString categoryName;
	u32      virtualMemory;
	u32      physicalMemory;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
class streamingMemoryResult
{
public:
	atString moduleName;
	streamingMemoryCategoryResult categories[ CDataFileMgr::CONTENTS_MAX ];

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

#define NUM_MEMORY_DISTRIBUTION_BUCKETS	(32)

class memoryDistributionResult
{
public:

	int idx;

	memoryUsageResult physicalUsedBySize;
	memoryUsageResult physicalFreeBySize;

	memoryUsageResult virtualUsedBySize;
	memoryUsageResult virtualFreeBySize;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class minMaxAverageResult
{
public:
	int min;
	int max;
	int average;

	PAR_SIMPLE_PARSABLE;
};

class pedAndVehicleStatResults
{
public:
	minMaxAverageResult	numPeds;
	minMaxAverageResult	numActivePeds;
	minMaxAverageResult	numInactivePeds;
	minMaxAverageResult	numEventScanHiPeds;
	minMaxAverageResult	numEventScanLoPeds;
	minMaxAverageResult	numMotionTaskHiPeds;
	minMaxAverageResult	numMotionTaskLoPeds;
	minMaxAverageResult	numPhysicsHiPeds;
	minMaxAverageResult	numPhysicsLoPeds;
	minMaxAverageResult	numEntityScanHiPeds;
	minMaxAverageResult	numEntityScanLoPeds;

	minMaxAverageResult	numVehicles;
	minMaxAverageResult	numActiveVehicles;
	minMaxAverageResult	numInactiveVehicles;
	minMaxAverageResult	numRealVehicles;
	minMaxAverageResult	numDummyVehicles;
	minMaxAverageResult	numSuperDummyVehicles;

	PAR_SIMPLE_PARSABLE;
};

class streamingModuleResult
{
public:
	int moduleId;
	atString moduleName;
	size_t loadedMemVirt;
	size_t loadedMemPhys;
	size_t requiredMemVirt;
	size_t requiredMemPhys;
	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
class metricsList
{
public:
	int idx;
	atString name;													// The name of the zone
	framesPerSecondResult * fpsResult;
	msPerFrameResult * msPFResult;
	atArray<gpuTimingResult*> gpuResults;
	atArray<cpuTimingResult*> cpuResults;
	atArray<threadTimingResult*> threadResults;
	atArray<memoryBucketUsageResult*> memoryResults;
	atArray<memoryHeapResult*> memoryHeapResults;
	atArray<streamingMemoryResult*> streamingMemoryResults;
	atArray<streamingStatsResult*> streamingStatsResults;
	atArray<streamingModuleResult*> streamingModuleResults;
	atArray<drawListResult*> drawListResults;
	atArray<memoryDistributionResult*> memDistributionResults;
	scriptMemoryUsageResult	*scriptMemResult;
	pedAndVehicleStatResults *pedAndVehicleResults;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
class debugLocationMetricsList
{
public:	
	atString platform;
	atString buildversion;
	atString changeList;
	size_t	 exeSize;
	atArray<metricsList*> results;

	PAR_SIMPLE_PARSABLE;
};


namespace MetricsCapture
{
	class CaptureInstance
	{
	public:
		CaptureInstance();
		debugLocationMetricsList * GetResults();
		int GetSamplingId();
		void Reset();
		bool Running();
		bool Sampling();
		void StartCapture();
		void StartSampling(int entryId, const char * entryName);
		void StopSampling();

		void SendToTelemetry();

	private:
		debugLocationMetricsList m_ResultList;
		bool m_Running;
		int m_SamplingId;
		const char * m_SamplingName;
	};

	extern	CaptureInstance g_ZoneCapture;

#endif // ENABLE_STATS_CAPTURE

};


#endif // INC_AUTO_GPU_CAPTURE_H_
