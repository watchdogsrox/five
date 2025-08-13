#include "AutoGPUCapture.h"

#include "grcore/device.h"
#include "grprofile/timebars.h"
#include "parser/manager.h"
#include "parser/visitorxml.h"
#include "system/param.h"
#include "system/memory.h"
#include "system/simpleallocator.h"


#include "fwdrawlist/drawlistmgr.h"
#include "fwsys/timer.h"

#include "debug/Debug.h"
#include "debug/DebugLocation.h"
#include "peds/ped.h"
#include "peds/pedpopulation.h"
#include "system/controlmgr.h"
#include "system/FileMgr.h"
#include "scene/world/GameWorld.h"
#include "streaming/streamingdebuggraph.h"
#include "text/text.h"
#include "text/textconversion.h"
#include "script/script.h"
#include "vehicles/vehiclepopulation.h"

#include "grmodel/setup.h"
#include <limits>

#if !__FINAL
#include "Network/Live/NetworkDebugTelemetry.h"
#endif

#if ENABLE_STATS_CAPTURE


//OPTIMISATIONS_OFF()


#include "AutoGPUCapture_parser.h"

extern	void GetLoadedSizesSafe(strStreamingModule &module, strLocalIndex objIndex, int &outVirtSize, int &outPhysSize);


////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace MetricsCapture
{

////////////////////////////////////////////////////////////////////////////////////////////////////
PARAM(startAutoMetricsCapture,"start automated metrics capture");
PARAM(startAutoGPUCapture,"start automated metrics capture"); // deprecated
PARAM(autocapturepath,"[system] Specify a path for the files stored by the automatic metrics capture");
PARAM(autocaptureCL, "Specify the CL used to generate this build");

#if __WIN32PC
PARAM(fpsCaptureUseUserFolder, "Force the output folder for the metrics xml files to be the user one");
PARAM(fpsCaptureExtraPath, "extra path to be appenned after the user folder");
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
#define DEBUG_METRICS_CAPTURES 0

//MUST match those values used in SmokeTest tools project (C#), which are based on RSG.Platform.RagebuilderPlatform
#define PLATFORM_STRING __XENON == 1 ? "xbox360" : (__PS3 == 1 ? "ps3" : (RSG_DURANGO == 1 ? "durango" : (RSG_ORBIS == 1 ? "orbis" : (RSG_CPU_X64 == 1 ? "win64" : (__WIN32 == 1 ? "win32" : "??")))))

// yuck
const int MEMBUCKET_SLOSH = MEMBUCKET_DEBUG + 1;


enum MemoryHeapIds
{
	MEMHEAP_INVALID = -1,
	MEMHEAP_GAME_VIRTUAL =  0,
	MEMHEAP_RESOURCE_VIRTUAL,
	MEMHEAP_DEBUG_VIRTUAL,
	MEMHEAP_STREAMING_VIRTUAL,
	MEMHEAP_EXTERNAL_VIRTUAL,
	MEMHEAP_SLOSH_VIRTUAL,
	MEMHEAP_SMALL_ALLOCATOR
};

////////////////////////////////////////////////////////////////////////////////////////////////////
static const int NOT_SAMPLING = -1;

////////////////////////////////////////////////////////////////////////////////////////////////////
class MutexHelper
{
public:
	MutexHelper()
	{
		rage::sysIpcLockMutex(sm_Mutex);
	}
	~MutexHelper()
	{
		rage::sysIpcUnlockMutex(sm_Mutex);
	}
public:
	static rage::sysIpcMutex sm_Mutex;
};

rage::sysIpcMutex MutexHelper::sm_Mutex = NULL;

////////////////////////////////////////////////////////////////////////////////////////////////////
class CMetricsCaptureWindow
{
public:
	CMetricsCaptureWindow(const debugLocationMetricsList & data)
		: m_Data(data)
		, m_Left(0.5f)
		, m_Right(0.95f)
		, m_Bottom(0.75f)
		, m_Top(0.5f)
		, m_Split(0.7f)
		, m_SplitPadding(0.01f)
		, m_ZoneTextSize(0.4f, 0.4f)
		, m_MetricTextSize(0.28f, 0.28f)
		, m_ScreenWidth(0.0f)
		, m_ScreenHeight(0.0f)
		, m_ZoneTextHeight(0.0f)
		, m_MetricTextHeight(0.0f)
		, m_PaddingBetweenZones(0.0f)
		, m_ScrollOffset(0.0f)
		, m_YPos(0.0f)
		, m_LastLength(0.0f)
	{
		m_LabelTextLayout.SetColor(Color32(255, 255, 255, 255));
		m_LabelTextLayout.SetOrientation(FONT_RIGHT);

		m_DataTextLayout.SetColor(Color32(255, 255, 255, 255));
		m_DataTextLayout.SetOrientation(FONT_LEFT);

		Move(0.5f, 0.5f);
	}

	void Draw()
	{
		m_ScrollOffset += CControlMgr::GetKeyboard().GetKeyDown(KEY_UP) ? 0.01f : 0.0f;
		m_ScrollOffset -= CControlMgr::GetKeyboard().GetKeyDown(KEY_DOWN) ? 0.01f : 0.0f;		
		m_ScrollOffset = Clamp(m_ScrollOffset, -(m_LastLength-(m_Bottom-m_Top)), 0.0f);
		m_YPos = m_Top + m_ScrollOffset;

		m_ScreenWidth = (float)grcViewport::GetDefaultScreen()->GetWidth();
		m_ScreenHeight = (float)grcViewport::GetDefaultScreen()->GetHeight();
		m_ZoneTextHeight = 24.0f / m_ScreenHeight;
		m_MetricTextHeight = 16.0f / m_ScreenHeight;
		m_PaddingBetweenZones = 16.0f / m_ScreenHeight;

		DrawBackground();		

		const debugLocationMetricsList * resultList = &m_Data;

		for(int zoneIndex = 0; zoneIndex < resultList->results.size(); ++zoneIndex)
		{
			metricsList * metrics = resultList->results[zoneIndex];

			BeginZone(metrics->name);

			if(metrics->fpsResult)
			{
				framesPerSecondResult * fpsResult = metrics->fpsResult;
				BeginMetric("Frames Per Second");
				DrawAverageMinMax("", fpsResult->average, fpsResult->min, fpsResult->max);
				EndMetric();
			}

			if(metrics->msPFResult)
			{
				msPerFrameResult *msPFResult = metrics->msPFResult;
				BeginMetric("Millseconds per Frame");
				DrawAverageMinMax("", msPFResult->average, msPFResult->min, msPFResult->max);
				EndMetric();
			}

			for(int threadResultIndex = 0; threadResultIndex < metrics->threadResults.size(); ++threadResultIndex)
			{
				threadTimingResult *threadResult = metrics->threadResults[threadResultIndex];
				snprintf(m_Text, MAX_TEXT_LENGTH, "%s", threadResult->name.c_str());
				BeginMetric(m_Text);
				DrawAverageMinMax("ms", threadResult->average, threadResult->min, threadResult->max);
				EndMetric();
			}

			for(int cpuResultIndex = 0; cpuResultIndex < metrics->cpuResults.size(); ++cpuResultIndex)
			{
				cpuTimingResult * cpuResult = metrics->cpuResults[cpuResultIndex];
				snprintf(m_Text, MAX_TEXT_LENGTH, "%s - %s", cpuResult->set.c_str(), cpuResult->name.c_str());
				BeginMetric(m_Text);
				DrawAverageMinMax("ms", cpuResult->average, cpuResult->min, cpuResult->max);
				EndMetric();
			}

			EndZone();
		}

		m_LastLength = m_YPos - (m_Top + m_ScrollOffset);
	}

	void Move(float x, float y)
	{
		m_LabelTextLayout.SetWrap(Vector2(0.0f, m_Split - m_SplitPadding));
		m_DataTextLayout.SetWrap(Vector2(m_Split + m_SplitPadding, m_Right));

		float width = m_Right - m_Left;
		float height = m_Bottom - m_Top;
		float splitDistance = m_Split - m_Left;

		m_Left = x;
		m_Right = x + width;
		m_Top = y;
		m_Bottom = y + height;
		m_Split = x + splitDistance;
	}

private:
	bool IsSpaceForRow(float rowHeight)
	{
		return (m_Bottom - m_YPos) >= rowHeight;
	}

	bool IsRowVisible(float rowHeight)
	{
		return m_YPos >= m_Top && IsSpaceForRow(rowHeight);
	}

	void DrawBackground()
	{
		fwRect bgRect(m_Left * m_ScreenWidth, m_Bottom * m_ScreenHeight, m_Right * m_ScreenWidth, m_Top * m_ScreenHeight);
		Color32 bgColour(0, 0, 0, 128);
		CSprite2d::DrawRectSlow(bgRect, bgColour);
	}

	void BeginZone(const char * name)
	{
		if(IsRowVisible(m_MetricTextHeight))
		{
			snprintf(m_Text, MAX_TEXT_LENGTH, "%s", name);
			m_LabelTextLayout.SetScale(m_ZoneTextSize);
			m_LabelTextLayout.Render(Vector2(m_Left, m_YPos), m_Text);
		}
		m_YPos += m_ZoneTextHeight;
	}

	void EndZone()
	{
		m_YPos += m_PaddingBetweenZones;
	}

	void BeginMetric(const char * name)
	{
		DrawDataLabel(name);
	}

	void EndMetric()
	{
		m_YPos += m_MetricTextHeight;
	}

	void DrawAverageMinMax(const char * suffix, float average, float min, float max)
	{
		snprintf(m_Text, MAX_TEXT_LENGTH, "%.1f%s [%.1f%s, %.1f%s]", average, suffix, min, suffix, max, suffix);
		DrawData(m_Text);
	}

	void DrawDataLabel(const char * text)
	{
		if(IsRowVisible(m_MetricTextHeight))
		{
			m_LabelTextLayout.SetScale(m_MetricTextSize);
			m_LabelTextLayout.Render(Vector2(m_Left, m_YPos), text);
		}
	}

	void DrawData(const char * text)
	{
		if(IsRowVisible(m_MetricTextHeight))
		{
			m_DataTextLayout.SetScale(m_MetricTextSize); //todo: why do we need to set scale here?
			m_DataTextLayout.Render(Vector2(m_Split + m_SplitPadding, m_YPos), text );
		}
	}

private:
	const debugLocationMetricsList & m_Data;

	float m_Left;
	float m_Right;
	float m_Bottom;
	float m_Top;
	float m_Split;
	float m_SplitPadding;
	Vector2 m_ZoneTextSize;
	Vector2 m_MetricTextSize;
	float m_ScreenWidth;
	float m_ScreenHeight;
	float m_ZoneTextHeight;
	float m_MetricTextHeight;
	float m_PaddingBetweenZones;
	float m_ScrollOffset;
	float m_YPos;
	float m_LastLength;

	static const int MAX_TEXT_LENGTH = 200;
	char m_Text[MAX_TEXT_LENGTH];
	CTextLayout m_LabelTextLayout;
	CTextLayout m_DataTextLayout;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
struct CPUTiming
{
	const char * name;
	int timingSet;
	bool optional;
	SampledValue<float> value;
	int callCountThisFrame;
	float timeThisFrame;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
struct ThreadTiming
{
	const char * name;
	int timingSet;
	SampledValue<float> value;
	float timeThisFrame;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
typedef atArray<CPUTiming> TimingArray;

////////////////////////////////////////////////////////////////////////////////////////////////////
typedef atArray<ThreadTiming> ThreadTimingArray;

////////////////////////////////////////////////////////////////////////////////////////////////////
struct MemoryBucketUsage
{
	MemoryBucketIds bucket;

	SampledValue<u32> gameVirtual;
	SampledValue<u32> gamePhysical;
	SampledValue<u32> resourceVirtual;
	SampledValue<u32> resourcePhysical;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
struct DrawListCount
{
	int id;
	SampledValue<u32> value;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
class ScriptMemoryUsage
{
public:
	SampledValue<u32> physicalMem;
	SampledValue<u32> virtualMem;
};

void InitScriptMemResult(ScriptMemoryUsage &result)
{
	result.physicalMem.Reset();
	result.virtualMem.Reset();
}

//	Do I need to wrap this entire function in #if __SCRIPT_MEM_CALC
void UpdateScriptMemResult( ScriptMemoryUsage &result )
{
	u32 phys = 0;
	u32 virt = 0;
#if __SCRIPT_MEM_CALC
	phys = CTheScripts::GetScriptHandlerMgr().GetTotalScriptPhysicalMemory();
	virt = CTheScripts::GetScriptHandlerMgr().GetTotalScriptVirtualMemory();
#endif	//	__SCRIPT_MEM_CALC
	result.physicalMem.AddSample(phys);
	result.virtualMem.AddSample(virt);
}

void StoreScriptMemResult(debugLocationMetricsList * resultList, const ScriptMemoryUsage &result)
{
	USE_DEBUG_MEMORY();

	if (Verifyf(resultList->results.GetCount(), "Script metric list doesn't have any storage for results yet - did we not call StartSampling?"))
	{
		scriptMemoryUsageResult * pResult = rage_new scriptMemoryUsageResult;

		pResult->physicalMem.average = result.physicalMem.Mean();
		pResult->physicalMem.min = result.physicalMem.Min();
		pResult->physicalMem.max = result.physicalMem.Max();
		pResult->physicalMem.std = result.physicalMem.StandardDeviation();
		pResult->virtualMem.average = result.virtualMem.Mean();
		pResult->virtualMem.min = result.virtualMem.Min();
		pResult->virtualMem.max = result.virtualMem.Max();
		pResult->virtualMem.std = result.virtualMem.StandardDeviation();
		resultList->results.Top()->scriptMemResult = pResult;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

class StreamingStats
{
public:
	SampledValue<u32>	requested;
	SampledValue<u32>	loaded;
};

typedef atArray<StreamingStats> StreamingStatsArray;

void InitStreamingStats(StreamingStatsArray &result)
{
	result.Reset();

#if TRACK_PLACE_STATS
	int modules = strStreamingEngine::GetInfo().GetModuleMgr().GetNumberOfModules();

	for (int i=0; i<modules; ++i)
	{
		strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(i);

		pModule->UpdatePlaceStats();
	}
#endif // TRACK_PLACE_STATS
}

void UpdateStreamingStats( StreamingStatsArray &TRACK_PLACE_STATS_ONLY(result) )
{
#if TRACK_PLACE_STATS
	int modules = strStreamingEngine::GetInfo().GetModuleMgr().GetNumberOfModules();

	if (result.GetCount() == 0)
	{
		USE_DEBUG_MEMORY();
		result.Resize(modules);
	}

	for (int i=0; i<modules; ++i)
	{
		strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(i);

		u32 requestedSize = (u32)pModule->GetRequestedSize();
		u32 loadedSize = (u32)pModule->GetLoadedSize();

		pModule->UpdatePlaceStats();

		result[i].loaded.AddSample(loadedSize);
		result[i].requested.AddSample(requestedSize);
	}
#endif // TRACK_PLACE_STATS
}

void StoreStreamingStats(debugLocationMetricsList * TRACK_PLACE_STATS_ONLY(resultList), const StreamingStatsArray &TRACK_PLACE_STATS_ONLY(result))
{
#if TRACK_PLACE_STATS
	USE_DEBUG_MEMORY();

	if (Verifyf(resultList->results.GetCount(), "Script metric list doesn't have any storage for results yet - did we not call StartSampling?"))
	{
		int modules = strStreamingEngine::GetInfo().GetModuleMgr().GetNumberOfModules();
		for (int i=0; i<modules; ++i)
		{
			strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(i);
			streamingStatsResult* pResult = rage_new streamingStatsResult;

			pResult->name = pModule->GetModuleName();

			pResult->avgLoaded = result[i].loaded.Mean();
			pResult->minLoaded = result[i].loaded.Min();
			pResult->maxLoaded = result[i].loaded.Max();
			pResult->stdLoaded = result[i].loaded.StandardDeviation();
			pResult->totalLoaded = result[i].loaded.Sum();

			pResult->avgRequested = result[i].requested.Mean();
			pResult->minRequested = result[i].requested.Min();
			pResult->maxRequested = result[i].requested.Max();
			pResult->stdRequested = result[i].requested.StandardDeviation();
			pResult->totalRequested = result[i].requested.Sum();

			resultList->results.Top()->streamingStatsResults.Grow() = pResult;
		}
	}
#endif // TRACK_PLACE_STATS
}



////////////////////////////////////////////////////////////////////////////////////////////////////



void StoreStreamingModuleMemory(debugLocationMetricsList * resultList )
{
	USE_DEBUG_MEMORY();

	if (Verifyf(resultList->results.GetCount(), "Streaming module metric list doesn't have any storage for results yet - did we not call StartSampling?"))
	{
		strStreamingInfoManager &strInfoManager = strStreamingEngine::GetInfo();
		strStreamingModuleMgr &moduleMgr = strStreamingEngine::GetInfo().GetModuleMgr();

		int numModules = moduleMgr.GetNumberOfModules();

		//stupidly slow
		for(int moduleId=0;moduleId<numModules;moduleId++)	
		{
			strStreamingModule *pModule = moduleMgr.GetModule(moduleId);
			streamingModuleResult* pResult = rage_new streamingModuleResult;

			pResult->moduleName = pModule->GetModuleName();
			pResult->moduleId = moduleId;
			pResult->loadedMemPhys = 0;
			pResult->loadedMemVirt = 0;
			pResult->requiredMemPhys = 0;
			pResult->requiredMemVirt = 0;

			// Types to track:- Loaded, Referenced/Required.
			for(s32 objIdx = 0; objIdx < pModule->GetCount(); ++objIdx)
			{
				if(pModule->IsObjectInImage(strLocalIndex(objIdx)))
				{
					strIndex streamingIndex = pModule->GetStreamingIndex(strLocalIndex(objIdx));
					strStreamingInfo *info = strStreamingEngine::GetInfo().GetStreamingInfo(streamingIndex);
					bool bRequired = pModule->IsObjectRequired(strLocalIndex(objIdx) );
					int iNumRefs = pModule->GetNumRefs(strLocalIndex(objIdx));

					// only count non "cached" modules
					if(	!info->IsFlagSet(STRFLAG_INTERNAL_DUMMY) &&
						(bRequired || iNumRefs > 0))
					{
						bool bLoaded  = strInfoManager.HasObjectLoaded(streamingIndex);
						bool bReferenced = bLoaded && ( iNumRefs > 0 || info->GetDependentCount() > 0 );
						
						int virt;	
						int phys;	

						GetLoadedSizesSafe(*pModule, strLocalIndex(objIdx), virt, phys );

						if(bLoaded)
						{
							pResult->loadedMemPhys += phys;
							pResult->loadedMemVirt += virt;
						}

						if(bReferenced || bRequired)
						{
							pResult->requiredMemPhys += phys;
							pResult->requiredMemVirt += virt;
						}
					}
				}
			}

			resultList->results.Top()->streamingModuleResults.Grow() = pResult;
		} 
	}
}



////////////////////////////////////////////////////////////////////////////////////////////////////

class MemoryDistributionUsage
{
public:
	SampledValue<u32> physicalUsedBySize;
	SampledValue<u32> physicalFreeBySize;

	SampledValue<u32> virtualUsedBySize;
	SampledValue<u32> virtualFreeBySize;
};

typedef atArray<MemoryDistributionUsage> MemoryDistributionArray;

void InitMemDistributionResult(MemoryDistributionArray &result)
{
	result.Reset();
}

void UpdateMemDistributionResult( MemoryDistributionArray &result )
{
	sysMemDistribution distV;

	sysMemAllocator::GetCurrent().GetAllocator(MEMTYPE_RESOURCE_VIRTUAL)->GetMemoryDistribution(distV);
#if __PS3 || RSG_PC
	sysMemDistribution distP;
	sysMemAllocator::GetCurrent().GetAllocator(MEMTYPE_RESOURCE_PHYSICAL)->GetMemoryDistribution(distP);
#endif // __PS3

	if (result.GetCount() == 0)
	{
		USE_DEBUG_MEMORY();
		result.Resize(NUM_MEMORY_DISTRIBUTION_BUCKETS);
	}

	for(int i=0;i<NUM_MEMORY_DISTRIBUTION_BUCKETS;i++)
	{
		result[i].virtualUsedBySize.AddSample(distV.UsedBySize[i]);
		result[i].virtualFreeBySize.AddSample(distV.FreeBySize[i]);

#if __PS3
		result[i].physicalUsedBySize.AddSample(distP.UsedBySize[i]);
		result[i].physicalFreeBySize.AddSample(distP.FreeBySize[i]);
#endif // __PS3
	}
}

void StoreMemDistributionResult(debugLocationMetricsList * resultList, const MemoryDistributionArray &result)
{
	USE_DEBUG_MEMORY();

	if (Verifyf(resultList->results.GetCount(), "Script metric list doesn't have any storage for results yet - did we not call StartSampling?"))
	{
		for(int i=0;i<result.GetCount();i++)
		{
			memoryDistributionResult * pResult = rage_new memoryDistributionResult;

			pResult->idx = i;

			pResult->physicalFreeBySize.average = result[i].physicalFreeBySize.Mean();
			pResult->physicalFreeBySize.min = result[i].physicalFreeBySize.Min();
			pResult->physicalFreeBySize.max = result[i].physicalFreeBySize.Max();
			pResult->physicalFreeBySize.std = result[i].physicalFreeBySize.StandardDeviation();

			pResult->physicalUsedBySize.average = result[i].physicalUsedBySize.Mean();
			pResult->physicalUsedBySize.min = result[i].physicalUsedBySize.Min();
			pResult->physicalUsedBySize.max = result[i].physicalUsedBySize.Max();
			pResult->physicalUsedBySize.std = result[i].physicalUsedBySize.StandardDeviation();

			pResult->virtualFreeBySize.average = result[i].virtualFreeBySize.Mean();
			pResult->virtualFreeBySize.min = result[i].virtualFreeBySize.Min();
			pResult->virtualFreeBySize.max = result[i].virtualFreeBySize.Max();
			pResult->virtualFreeBySize.std = result[i].virtualFreeBySize.StandardDeviation();

			pResult->virtualUsedBySize.average = result[i].virtualUsedBySize.Mean();
			pResult->virtualUsedBySize.min = result[i].virtualUsedBySize.Min();
			pResult->virtualUsedBySize.max = result[i].virtualUsedBySize.Max();
			pResult->virtualUsedBySize.std = result[i].virtualUsedBySize.StandardDeviation();

			resultList->results.Top()->memDistributionResults.Grow() = pResult;
		}

	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

class pedAndVehicleStatUsage
{
public:

	SampledValue<u32> numPeds;
	SampledValue<u32> numActivePeds;
	SampledValue<u32> numInactivePeds;
	SampledValue<u32> numEventScanHiPeds;
	SampledValue<u32> numEventScanLoPeds;
	SampledValue<u32> numMotionTaskHiPeds;
	SampledValue<u32> numMotionTaskLoPeds;
	SampledValue<u32> numPhysicsHiPeds;
	SampledValue<u32> numPhysicsLoPeds;
	SampledValue<u32> numEntityScanHiPeds;
	SampledValue<u32> numEntityScanLoPeds;

	SampledValue<u32> numVehicles;
	SampledValue<u32> numActiveVehicles;
	SampledValue<u32> numInactiveVehicles;
	SampledValue<u32> numRealVehicles;
	SampledValue<u32> numDummyVehicles;
	SampledValue<u32> numSuperDummyVehicles;
};


void InitPedAndVehicleStatResult(pedAndVehicleStatUsage &result)
{
	result.numPeds.Reset();
	result.numActivePeds.Reset();
	result.numInactivePeds.Reset();
	result.numEventScanHiPeds.Reset();
	result.numEventScanLoPeds.Reset();
	result.numMotionTaskHiPeds.Reset();
	result.numMotionTaskLoPeds.Reset();
	result.numPhysicsHiPeds.Reset();
	result.numPhysicsLoPeds.Reset();
	result.numEntityScanHiPeds.Reset();
	result.numEntityScanLoPeds.Reset();

	result.numVehicles.Reset();
	result.numActiveVehicles.Reset();
	result.numInactiveVehicles.Reset();
	result.numRealVehicles.Reset();
	result.numDummyVehicles.Reset();
	result.numSuperDummyVehicles.Reset();
}

void UpdatePedAndVehicleStatResult( pedAndVehicleStatUsage &result )
{
	// Peds
	int iNumPeds = 0;
	int iNumEventScanHi = 0;
	int iNumEventScanLo = 0;
	int iNumMotionTaskHi = 0;
	int iNumMotionTaskLo = 0;
	int iNumPhysicsHi = 0;
	int iNumPhysicsLo = 0;
	int iNumEntityScanHi = 0;
	int iNumEntityScanLo = 0;
	int iNumActive = 0;
	int iNumInactive = 0;
#if DEBUG_DRAW
	int iNumTimeslicedHiPeds = 0;
	int iNumTimeslicedLoPeds = 0;
	CPedPopulation::GetPedLODCounts(iNumPeds, iNumEventScanHi, iNumEventScanLo, iNumMotionTaskHi, iNumMotionTaskLo, iNumPhysicsHi, iNumPhysicsLo, iNumEntityScanHi, iNumEntityScanLo, iNumActive, iNumInactive, iNumTimeslicedHiPeds, iNumTimeslicedLoPeds );
#endif //DEBUG_DRAW

	result.numPeds.AddSample(iNumPeds);
	result.numActivePeds.AddSample(iNumActive);
	result.numInactivePeds.AddSample(iNumInactive);
	result.numEventScanHiPeds.AddSample(iNumEventScanHi);
	result.numEventScanLoPeds.AddSample(iNumEventScanLo);
	result.numMotionTaskHiPeds.AddSample(iNumMotionTaskHi);
	result.numMotionTaskLoPeds.AddSample(iNumMotionTaskLo);
	result.numPhysicsHiPeds.AddSample(iNumPhysicsHi);
	result.numPhysicsLoPeds.AddSample(iNumPhysicsLo);
	result.numEntityScanHiPeds.AddSample(iNumEntityScanHi);
	result.numEntityScanLoPeds.AddSample(iNumEntityScanLo);

	// Vehicles
	int iTotalNumVehicles = 0;
	int iNumRealVehicles = 0;
	int iNumDummyVehicles = 0;
	int iNumSuperDummyVehicles = 0;
	int iNumActiveVehicles = 0;
	int iNumInactiveVehicles = 0;
#if DEBUG_DRAW
	int iNumTimeslicedVehicles = 0;
	CVehiclePopulation::GetVehicleLODCounts(iTotalNumVehicles, iNumRealVehicles, iNumDummyVehicles, iNumSuperDummyVehicles, iNumActiveVehicles, iNumInactiveVehicles, iNumTimeslicedVehicles );
#endif //DEBUG_DRAW

	result.numVehicles.AddSample(iTotalNumVehicles);
	result.numActiveVehicles.AddSample(iNumActiveVehicles);
	result.numInactiveVehicles.AddSample(iNumInactiveVehicles);
	result.numRealVehicles.AddSample(iNumRealVehicles);
	result.numDummyVehicles.AddSample(iNumDummyVehicles);
	result.numSuperDummyVehicles.AddSample(iNumSuperDummyVehicles);
}


void StorePedAndVehicleStatResult(debugLocationMetricsList * resultList, const pedAndVehicleStatUsage &result )
{
	USE_DEBUG_MEMORY();

	if (Verifyf(resultList->results.GetCount(), "PedAndVehicleStat doesn't have any storage for results yet - did we not call StartSampling?"))
	{
		pedAndVehicleStatResults *pResult = rage_new pedAndVehicleStatResults;

		// Peds
		pResult->numPeds.min = result.numPeds.Min();
		pResult->numPeds.max = result.numPeds.Max();
		pResult->numPeds.average = result.numPeds.Mean();

		pResult->numActivePeds.min = result.numActivePeds.Min();
		pResult->numActivePeds.max = result.numActivePeds.Max();
		pResult->numActivePeds.average = result.numActivePeds.Mean();

		pResult->numInactivePeds.min = result.numInactivePeds.Min();
		pResult->numInactivePeds.max = result.numInactivePeds.Max();
		pResult->numInactivePeds.average = result.numInactivePeds.Mean();

		pResult->numEventScanHiPeds.min = result.numEventScanHiPeds.Min();
		pResult->numEventScanHiPeds.max = result.numEventScanHiPeds.Max();
		pResult->numEventScanHiPeds.average = result.numEventScanHiPeds.Mean();

		pResult->numEventScanLoPeds.min = result.numEventScanLoPeds.Min();
		pResult->numEventScanLoPeds.max = result.numEventScanLoPeds.Max();
		pResult->numEventScanLoPeds.average = result.numEventScanLoPeds.Mean();

		pResult->numMotionTaskHiPeds.min = result.numMotionTaskHiPeds.Min();
		pResult->numMotionTaskHiPeds.max = result.numMotionTaskHiPeds.Max();
		pResult->numMotionTaskHiPeds.average = result.numMotionTaskHiPeds.Mean();

		pResult->numMotionTaskLoPeds.min = result.numMotionTaskLoPeds.Min();
		pResult->numMotionTaskLoPeds.max = result.numMotionTaskLoPeds.Max();
		pResult->numMotionTaskLoPeds.average = result.numMotionTaskLoPeds.Mean();

		pResult->numPhysicsHiPeds.min = result.numPhysicsHiPeds.Min();
		pResult->numPhysicsHiPeds.max = result.numPhysicsHiPeds.Max();
		pResult->numPhysicsHiPeds.average = result.numPhysicsHiPeds.Mean();

		pResult->numPhysicsLoPeds.min = result.numPhysicsLoPeds.Min();
		pResult->numPhysicsLoPeds.max = result.numPhysicsLoPeds.Max();
		pResult->numPhysicsLoPeds.average = result.numPhysicsLoPeds.Mean();

		pResult->numEntityScanHiPeds.min = result.numEntityScanHiPeds.Min();
		pResult->numEntityScanHiPeds.max = result.numEntityScanHiPeds.Max();
		pResult->numEntityScanHiPeds.average = result.numEntityScanHiPeds.Mean();

		pResult->numEntityScanLoPeds.min = result.numEntityScanLoPeds.Min();
		pResult->numEntityScanLoPeds.max = result.numEntityScanLoPeds.Max();
		pResult->numEntityScanLoPeds.average = result.numEntityScanLoPeds.Mean();


		// Vehicles
		pResult->numVehicles.min = result.numVehicles.Min();
		pResult->numVehicles.max = result.numVehicles.Max();
		pResult->numVehicles.average = result.numVehicles.Mean();

		pResult->numActiveVehicles.min = result.numActiveVehicles.Min();
		pResult->numActiveVehicles.max = result.numActiveVehicles.Max();
		pResult->numActiveVehicles.average = result.numActiveVehicles.Mean();

		pResult->numInactiveVehicles.min = result.numInactiveVehicles.Min();
		pResult->numInactiveVehicles.max = result.numInactiveVehicles.Max();
		pResult->numInactiveVehicles.average = result.numInactiveVehicles.Mean();

		pResult->numRealVehicles.min = result.numRealVehicles.Min();
		pResult->numRealVehicles.max = result.numRealVehicles.Max();
		pResult->numRealVehicles.average = result.numRealVehicles.Mean();

		pResult->numDummyVehicles.min = result.numDummyVehicles.Min();
		pResult->numDummyVehicles.max = result.numDummyVehicles.Max();
		pResult->numDummyVehicles.average = result.numDummyVehicles.Mean();

		pResult->numSuperDummyVehicles.min = result.numSuperDummyVehicles.Min();
		pResult->numSuperDummyVehicles.max = result.numSuperDummyVehicles.Max();
		pResult->numSuperDummyVehicles.average = result.numSuperDummyVehicles.Mean();

		resultList->results.Top()->pedAndVehicleResults = pResult;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
const char* TimingSetNameFromIndex(int timingSet)
{
#if RAGE_TIMEBARS
	return g_pfTB.GetSetName(timingSet);
#else
	UNUSED_VAR(timingSet);
	return "";
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void InitFramesPerSecondResult(SampledValue<float> * fpsResult)
{
	Assert(fpsResult);
	fpsResult->Reset();
}

void UpdateFramesPerSecondResult(SampledValue<float> * fpsResult)
{
	Assert(fpsResult);
	float fps = 1.0f / rage::Max(0.001f, fwTimer::GetSystemTimeStep());
	fpsResult->AddSample(fps);
}

void StoreFramesPerSecondResult(debugLocationMetricsList * resultList, const SampledValue<float> & fpsResult)
{
	USE_DEBUG_MEMORY();

	if (Verifyf(resultList->results.GetCount(), "FPS metric list doesn't have any storage for results yet - did we not call StartSampling?"))
	{
		framesPerSecondResult * result = rage_new framesPerSecondResult;
		result->average = fpsResult.Mean();
		result->min = fpsResult.Min();
		result->max = fpsResult.Max();
		result->std = fpsResult.StandardDeviation();
		resultList->results.Top()->fpsResult = result;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void InitMSPerFrameResult(SampledValue<float> * msPFResult)
{
	Assert(msPFResult);
	msPFResult->Reset();
}

void UpdateMSPerFrameResult(SampledValue<float> * msPFResult)
{
	Assert(msPFResult);
	float msPF = static_cast<float>(fwTimer::GetSystemTimeStepInMilliseconds());
	msPFResult->AddSample(msPF);
}

void StoreMSPerFrameResult(debugLocationMetricsList * resultList, const SampledValue<float> & msPFResult)
{
	USE_DEBUG_MEMORY();

	if (Verifyf(resultList->results.GetCount(), "MS Per Frame metric list doesn't have any storage for results yet - did we not call StartSampling?"))
	{
		msPerFrameResult * result = rage_new msPerFrameResult;
		result->average = msPFResult.Mean();
		result->min = msPFResult.Min();
		result->max = msPFResult.Max();
		result->std = msPFResult.StandardDeviation();
		resultList->results.Top()->msPFResult = result;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsValidCPUTiming(const CPUTiming & cpuTiming)
{
    return cpuTiming.name != NULL && cpuTiming.timingSet != -1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void InitCPUTimingResults(TimingArray &cpuTimings)
{
	cpuTimings.Reset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void InitThreadTimingResults(ThreadTimingArray &threadTimings)
{
	int count = threadTimings.GetCount();

	for(int i = 0; i<count; ++i)
	{
		ThreadTiming & timing = threadTimings[i];
		timing.value.Reset();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UpdateCPUTimingResults(TimingArray& RAGE_TIMEBARS_ONLY(cpuTimings))
{
#if RAGE_TIMEBARS
	USE_DEBUG_MEMORY();

	pfTimeBars *startupBar = &g_pfTB.GetStartupBar();
	int timingCount = cpuTimings.GetCount();

	// Let's reset them all first.
	for (int x = 0; x<timingCount; x++)
	{
		cpuTimings[x].timeThisFrame = 0.0f;
		cpuTimings[x].callCountThisFrame = 0;
	}

	for (int x=0; x<g_pfTB.GetTimebarCount(); x++)
	{
		const pfTimeBars &timebar = g_pfTB.GetTimeBar(x);
		int nextElement = 0;

		// Exclude the startup bar.
		if (&timebar != startupBar)
		{
			// Get the one timebar frame that just finished processing.
			const pfTimeBars::sTimebarFrame &frame = timebar.GetRenderTimebarFrame();

			int elements = frame.m_number;

			for (int y=0; y<elements; y++)
			{
				const pfTimeBars::sFuncTime &time = frame.m_pTimes[y];

				// Ignore all detail bars.
				if (time.detail)
				{
					continue;
				}

				// The structure should match. Let's find the matching timing info.
				CPUTiming *timing = NULL;

				if (cpuTimings.GetCount() > nextElement)
				{
					for (int z=nextElement; z<timingCount; z++)
					{
						if (cpuTimings[z].timingSet == x && cpuTimings[z].name == time.nameptr)
						{
							// Found it.
							timing = &cpuTimings[z];
							nextElement = z;
							break;
						}
					}

					if (!timing)
					{
						// Maybe we skipped it earlier - let's try the ones before us too.
						for (int z=nextElement-1; z>=0; z--)
						{
							if (cpuTimings[z].timingSet == x && cpuTimings[z].name == time.nameptr)
							{
								// Found it.
								timing = &cpuTimings[z];
								nextElement = z;
								break;
							}
						}
					}
				}

				// If it's not here, then we need to create a new one.
				if (!timing)
				{
					timing = &(cpuTimings.Grow(128));
					timing->name = time.nameptr;
					timing->optional = false;
					timing->timingSet = x;
					timing->timeThisFrame = 0.0f;
					timing->callCountThisFrame = 0;
					timingCount++;
				}

				Assert(timing->timeThisFrame >= 0.0f);
				timing->timeThisFrame += time.endTime - time.startTime;
				Assert(timing->timeThisFrame >= 0.0f);
				timing->callCountThisFrame++;
			}
		}
	}

	// Now that we gathered the information this frame locally, let's actually add samples.
	for (int x=0; x<timingCount; x++)
	{
		CPUTiming &timing = cpuTimings[x];

		if (timing.callCountThisFrame)
		{
			timing.value.AddSample(timing.timeThisFrame);
			Assert(timing.value.Min() >= 0.0f);
		}
	}
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UpdateThreadTimingResults(ThreadTimingArray& threadTimings)
{
#if RAGE_TIMEBARS
	USE_DEBUG_MEMORY();

	if (threadTimings.GetCount() == 0)
	{
		threadTimings.Resize(3 + MULTIPLE_RENDER_THREADS);

		threadTimings[0].name = "Update Thread";
		threadTimings[1].name = "Render Thread";
		threadTimings[2].name = "Total GPU";

		#if MULTIPLE_RENDER_THREADS
			static char renderThreadNames[MULTIPLE_RENDER_THREADS][32];
			for(int i=0; i<MULTIPLE_RENDER_THREADS; ++i)
			{
				formatf(renderThreadNames[i], "Subrender Thread %d", i);
				threadTimings[i+3].name = renderThreadNames[i];
			}
		#endif // MULTIPLE_RENDER_THREADS
	}

	threadTimings[0].timeThisFrame = g_pfTB.GetTimeBar(1).GetRenderTimebarFrame().ComputeFrameTime();
	threadTimings[1].timeThisFrame = g_pfTB.GetTimeBar(2).GetRenderTimebarFrame().ComputeFrameTime();
	threadTimings[2].timeThisFrame = g_pfTB.GetTimeBar(3).GetRenderTimebarFrame().ComputeFrameTime();
	for(int i=0; i<MULTIPLE_RENDER_THREADS; ++i)
	{
		threadTimings[i+3].timeThisFrame = g_pfTB.GetTimeBar(i+4).GetRenderTimebarFrame().ComputeFrameTime();
	}

	// Now that we gathered the information this frame locally, let's actually add samples.
	for (int x=0; x<threadTimings.GetCount(); x++)
	{
		ThreadTiming &timing = threadTimings[x];
		timing.value.AddSample(timing.timeThisFrame);
	}

#else
	USE_DEBUG_MEMORY();

	if (threadTimings.GetCount() == 0)
	{
		threadTimings.Resize(3);

		threadTimings[0].name = "Update Thread";
		threadTimings[1].name = "Render Thread";
		threadTimings[2].name = "Total GPU";
	}

	threadTimings[0].timeThisFrame = grcSetupInstance->GetUpdateTime();
	threadTimings[1].timeThisFrame = grcSetupInstance->GetDrawTime();
	threadTimings[2].timeThisFrame = grcSetupInstance->GetGpuTime();

	// Now that we gathered the information this frame locally, let's actually add samples.
	for (int x=0; x<threadTimings.GetCount(); x++)
	{
		ThreadTiming &timing = threadTimings[x];
		timing.value.AddSample(timing.timeThisFrame);
	}

#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void StoreCPUTimingResults(debugLocationMetricsList * resultList, const TimingArray &cpuTimings)
{
	USE_DEBUG_MEMORY();

	if (Verifyf(resultList->results.GetCount(), "CPU metric list doesn't have any storage for results yet - did we not call StartSampling?"))
	{
		int count = cpuTimings.GetCount();
		for(int i = 0; i<count; ++i)
		{
			const CPUTiming & timing = cpuTimings[i];
			cpuTimingResult *result = rage_new cpuTimingResult;
			result->idx = i;
			result->name = timing.name;
			result->set = TimingSetNameFromIndex(timing.timingSet);
			result->average = timing.value.Mean();
			result->min = timing.value.Min();
			result->max = timing.value.Max();
			result->std = timing.value.StandardDeviation();
			resultList->results.Top()->cpuResults.Grow() = result;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void StoreThreadTimingResults(debugLocationMetricsList * resultList, const ThreadTimingArray &threadTimings)
{
	USE_DEBUG_MEMORY();

	if (Verifyf(resultList->results.GetCount(), "Thread metric list doesn't have any storage for results yet - did we not call StartSampling?"))
	{
		int count = threadTimings.GetCount();
		for(int i = 0; i<count; ++i)
		{
			const ThreadTiming & timing = threadTimings[i];
			threadTimingResult *result = rage_new threadTimingResult();
			result->idx = i;
			result->name = timing.name;
			result->average = timing.value.Mean();
			result->min = timing.value.Min();
			result->max = timing.value.Max();
			result->std = timing.value.StandardDeviation();
			resultList->results.Top()->threadResults.Grow() = result;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsValidMemoryBucketUsage(const MemoryBucketUsage & memoryBucketUsage)
{
    return memoryBucketUsage.bucket != MEMBUCKET_INVALID;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void StoreMemoryUsageToResult(const SampledValue<u32> & usage, memoryUsageResult & result)
{
    result.average = usage.Mean();
    result.min = usage.Min();
    result.max = usage.Max();
	result.std = usage.StandardDeviation();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void InitMemoryBucketUsageResults(MemoryBucketUsage * memoryBucketUsages)
{
    for(int i = 0; IsValidMemoryBucketUsage(memoryBucketUsages[i]); ++i)
    {
        MemoryBucketUsage & memoryBucketUsage = memoryBucketUsages[i];
        memoryBucketUsage.gameVirtual.Reset();
        memoryBucketUsage.gamePhysical.Reset();
        memoryBucketUsage.resourceVirtual.Reset();
        memoryBucketUsage.resourcePhysical.Reset();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UpdateMemoryBucketUsageResults(MemoryBucketUsage * memoryBucketUsages)
{
    for(int i = 0; IsValidMemoryBucketUsage(memoryBucketUsages[i]); ++i)
    {
        MemoryBucketUsage & memoryBucketUsage = memoryBucketUsages[i];
        sysMemAllocator & master = sysMemAllocator::GetMaster();
        int bucket = memoryBucketUsage.bucket;

		u32 gameVirtualKB;
		u32 gamePhysicalKB;
		u32 resourceVirtualKB; 
		u32 resourcePhysicalKB;
		if(bucket != MEMBUCKET_SLOSH)
		{
			gameVirtualKB = (u32)master.GetAllocator(MEMTYPE_GAME_VIRTUAL)->GetMemoryUsed(bucket)/1024;
			gamePhysicalKB = (u32)master.GetAllocator(MEMTYPE_GAME_PHYSICAL)->GetMemoryUsed(bucket)/1024;
			resourceVirtualKB = (u32)master.GetAllocator(MEMTYPE_RESOURCE_VIRTUAL)->GetMemoryUsed(bucket)/1024;
			resourcePhysicalKB = (u32)master.GetAllocator(MEMTYPE_RESOURCE_PHYSICAL)->GetMemoryUsed(bucket)/1024;
		}
		else
		{
			gameVirtualKB = 0;
			gamePhysicalKB = 0;
			resourceVirtualKB = (u32)strStreamingEngine::GetAllocator().GetVirtualSlosh();
			resourcePhysicalKB = (u32)strStreamingEngine::GetAllocator().GetPhysicalSlosh();
		}
        memoryBucketUsage.gameVirtual.AddSample(gameVirtualKB);
        memoryBucketUsage.gamePhysical.AddSample(gamePhysicalKB);
        memoryBucketUsage.resourceVirtual.AddSample(resourceVirtualKB);
        memoryBucketUsage.resourcePhysical.AddSample(resourcePhysicalKB);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void StoreMemoryBucketUsageResults(debugLocationMetricsList * resultList, const MemoryBucketUsage * memoryBucketUsages)
{
	USE_DEBUG_MEMORY();

	if (Verifyf(resultList->results.GetCount(), "Memory bucket metric list doesn't have any storage for results yet - did we not call StartSampling?"))
	{
		for(int i = 0; IsValidMemoryBucketUsage(memoryBucketUsages[i]); ++i)
		{
			const MemoryBucketUsage & memoryBucketUsage = memoryBucketUsages[i];
			memoryBucketUsageResult *result = rage_new memoryBucketUsageResult;
			result->idx = i;
			int bucket = memoryBucketUsage.bucket;

			// yuck; I had something like #define MEMBUCKET_SLOSH  (MEMBUCKET_DEBUG + 1) but ng doesnt like it
			if(bucket != MEMBUCKET_SLOSH)
			{
				result->name = sysMemGetBucketName((MemoryBucketIds)memoryBucketUsage.bucket);
			}
			else
			{
				result->name = "Slosh";
			}
			StoreMemoryUsageToResult(memoryBucketUsage.gameVirtual, result->gameVirtual);
			StoreMemoryUsageToResult(memoryBucketUsage.gamePhysical, result->gamePhysical);
			StoreMemoryUsageToResult(memoryBucketUsage.resourceVirtual, result->resourceVirtual);
			StoreMemoryUsageToResult(memoryBucketUsage.resourcePhysical, result->resourcePhysical);
			resultList->results.Top()->memoryResults.Grow() = result;
		}
	}
}

void GetMemoryHeapNumbers(memoryHeapResult* pResult, int heapIndex)
{
	sysMemAllocator* pAllocator = sysMemAllocator::GetMaster().GetAllocator(heapIndex);
	if(pAllocator && pResult)
	{
		pResult->used = pAllocator->GetMemoryUsed();
		pResult->free = pAllocator->GetMemoryAvailable();
		pResult->total = pAllocator->GetHeapSize();
		pResult->peak = pAllocator->GetHighWaterMark(false);
		pResult->fragmentation = pAllocator->GetFragmentation();
	}
}
	
void GetStreamingHeapNumbers(size_t& used, size_t& total)
{
	// Streaming Virtual
#if ONE_STREAMING_HEAP
	used = (size_t) strStreamingEngine::GetAllocator().GetPhysicalMemoryUsed();
	total = (size_t) strStreamingEngine::GetAllocator().GetPhysicalMemoryAvailable();
#else
	used = (size_t) strStreamingEngine::GetAllocator().GetVirtualMemoryUsed();
	total = (size_t) strStreamingEngine::GetAllocator().GetVirtualMemoryAvailable();
#endif
	WIN32PC_ONLY(used = strStreamingEngine::GetAllocator().GetMemoryUsed());
	WIN32PC_ONLY(total = strStreamingEngine::GetAllocator().GetMemoryAvailable());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void StoreMemHeapResults(debugLocationMetricsList * resultList )
{
	USE_DEBUG_MEMORY();

	if (Verifyf(resultList->results.GetCount(), "Memory heap metric list doesn't have any storage for results yet - did we not call StartSampling?"))
	{
		for(int i = MEMHEAP_GAME_VIRTUAL; i <= MEMHEAP_SMALL_ALLOCATOR; ++i)
		{
			memoryHeapResult *result = rage_new memoryHeapResult;
			result->idx = i;
			result->used = 0;
			result->free = 0;
			result->total = 0;
			result->peak = 0;
			result->fragmentation = 0;
			
			switch(i)
			{
			case MEMHEAP_GAME_VIRTUAL:
				{
					GetMemoryHeapNumbers(result, MEMTYPE_GAME_VIRTUAL);
					result->name = "GameVirtual";
				}
				break;

			case MEMHEAP_RESOURCE_VIRTUAL:
				{
					GetMemoryHeapNumbers(result, MEMTYPE_RESOURCE_VIRTUAL);
					result->name = "ResourceVirtual";
				}
				break;

			case MEMHEAP_DEBUG_VIRTUAL:
				{
					GetMemoryHeapNumbers(result, MEMTYPE_DEBUG_VIRTUAL);
					result->name = "DebugVirtual";
				}
				break;

			case MEMHEAP_STREAMING_VIRTUAL:
				{
					GetStreamingHeapNumbers(result->used, result->total);				
					result->name = "StreamingVirtual";
					result->free = result->total - result->used;
				}
				break;
			case MEMHEAP_EXTERNAL_VIRTUAL:
				{
#if ONE_STREAMING_HEAP
					result->used = (s32) strStreamingEngine::GetAllocator().GetExternalPhysicalMemoryUsed();	
#else
					result->used = (s32) strStreamingEngine::GetAllocator().GetExternalVirtualMemoryUsed();
#endif
					result->name = "ExternalVirtual";
				}
				break;
			case MEMHEAP_SLOSH_VIRTUAL:
				{
					sysMemAllocator* pAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_VIRTUAL);
					result->name = "SloshVirtual";
					if(pAllocator)
					{
						size_t streamingUsed, streamingTotal;
						GetStreamingHeapNumbers(streamingUsed, streamingTotal);	
						size_t streamingFree = streamingTotal - streamingUsed;
						result->used = pAllocator->GetMemoryAvailable() - streamingFree;
					}
				}
				break;
			case MEMHEAP_SMALL_ALLOCATOR:
				{
					sysMemAllocator* pAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL);
					result->name = "SmallAllocator";
					if(pAllocator)
					{
						sysMemSimpleAllocator* pSimpleAllocator = reinterpret_cast<sysMemSimpleAllocator*>(pAllocator);
						result->used = pSimpleAllocator->GetSmallocatorTotalSize();
					}
				}
				break;
			}
			resultList->results.Top()->memoryHeapResults.Grow() = result;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsValidDrawListCount(const DrawListCount & drawListCount)
{
    return drawListCount.id != -1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void InitDrawListResults(DrawListCount * drawListCounts)
{
    for(int i = 0; IsValidDrawListCount(drawListCounts[i]); ++i)
    {
        DrawListCount & count = drawListCounts[i];
        count.value.Reset();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UpdateDrawListResults(DrawListCount * drawListCounts)
{
    for(int i = 0; IsValidDrawListCount(drawListCounts[i]); ++i)
    {
        DrawListCount & count = drawListCounts[i];
        int drawListCount = gDrawListMgr->GetDrawListEntityCount(count.id);

        count.value.AddSample(drawListCount);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void StoreDrawListResults(debugLocationMetricsList * resultList, const DrawListCount * drawListCounts)
{
	USE_DEBUG_MEMORY();

	if (Verifyf(resultList->results.GetCount(), "Draw list metric list doesn't have any storage for results yet - did we not call StartSampling?"))
	{
		for(int i = 0; IsValidDrawListCount(drawListCounts[i]); ++i)
		{
			const DrawListCount & count = drawListCounts[i];
			drawListResult *result = rage_new drawListResult;
			result->idx = i;
			result->name = gDrawListMgr->GetDrawListName(count.id);
			result->average = count.value.Mean();
			result->min = count.value.Min();
			result->max = count.value.Max();
			result->std = count.value.StandardDeviation();
			resultList->results.Top()->drawListResults.Grow() = result;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void ClearResults(debugLocationMetricsList * resultList)
{
	Assert(resultList);

	for(int i = 0; i < resultList->results.size(); ++i)
	{
		metricsList * metrics = resultList->results[i];

		if(metrics->fpsResult)
		{
			delete metrics->fpsResult;
			metrics->fpsResult = NULL;
		}

		if(metrics->msPFResult)
		{
			delete metrics->msPFResult;
			metrics->msPFResult = NULL;
		}

		for(int gpuResultIndex = 0; gpuResultIndex < metrics->gpuResults.size(); ++gpuResultIndex)
		{
			delete metrics->gpuResults[gpuResultIndex];
		}
		for(int cpuResultIndex = 0; cpuResultIndex < metrics->cpuResults.size(); ++cpuResultIndex)
		{
			delete metrics->cpuResults[cpuResultIndex];
		}
		for(int memoryResultIndex = 0; memoryResultIndex < metrics->memoryResults.size(); ++memoryResultIndex)
		{
			delete metrics->memoryResults[memoryResultIndex];
		}
		for(int streamingMemoryResultIndex = 0; streamingMemoryResultIndex < metrics->streamingMemoryResults.size(); ++streamingMemoryResultIndex)
		{
			delete metrics->streamingMemoryResults[streamingMemoryResultIndex];
		}
		for(int drawListResultIndex = 0; drawListResultIndex < metrics->drawListResults.size(); ++drawListResultIndex)
		{
			delete metrics->drawListResults[drawListResultIndex];
		}
		if(metrics->scriptMemResult)
		{
			delete metrics->scriptMemResult;
			metrics->scriptMemResult = NULL;
		}
		for(int memDistributionResultIndex = 0; memDistributionResultIndex < metrics->memDistributionResults.size(); ++memDistributionResultIndex)
		{
			delete metrics->memDistributionResults[memDistributionResultIndex];
		}
		if(metrics->pedAndVehicleResults)
		{
			delete metrics->pedAndVehicleResults;
			metrics->pedAndVehicleResults = NULL;
		}
		for(int streamStats = 0; streamStats < metrics->streamingStatsResults.size(); ++streamStats)
		{
			delete metrics->streamingStatsResults[streamStats];
		}
		delete metrics;
	}
	resultList->results.clear();
}

CaptureInstance::CaptureInstance()
{
	m_ResultList.platform = PLATFORM_STRING;
	Reset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
debugLocationMetricsList * CaptureInstance::GetResults()
{
	return &m_ResultList;
}

int CaptureInstance::GetSamplingId()
{
	return m_SamplingId;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void CaptureInstance::Reset()
{
	ClearResults(&m_ResultList);
	m_Running = false;
	m_SamplingId = NOT_SAMPLING;
	m_SamplingName = "";
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool CaptureInstance::Running()
{
	return m_Running;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool CaptureInstance::Sampling()
{
	return m_SamplingId != NOT_SAMPLING;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void CaptureInstance::StartCapture()
{
	Assert(!Running());
	m_Running = true;

	// Needs to be done here since the version number isn't available at time of captureinstance creation
	m_ResultList.buildversion = CDebug::GetVersionNumber();

	m_ResultList.exeSize = MEMMANAGER.GetExeSize();

	const char *cl = NULL;
	if(PARAM_autocaptureCL.Get(cl))
	{
		m_ResultList.changeList = cl;
	}
	else
	{
		m_ResultList.changeList = "specify with -autocaptureCL";
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void CaptureInstance::StartSampling(int samplingId, const char * samplingName)
{
	USE_DEBUG_MEMORY();

	Assert(Running());
	Assert(samplingId != NOT_SAMPLING);

	m_SamplingId = samplingId;
	m_SamplingName = samplingName;
	
	metricsList *list = rage_new metricsList;
	list->idx = samplingId;
	list->name = samplingName;
	list->fpsResult = NULL;
	list->msPFResult = NULL;
	list->scriptMemResult = NULL;
	list->pedAndVehicleResults = NULL;
	m_ResultList.results.Grow() = list;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void CaptureInstance::StopSampling()
{
	Assert(Running());
	m_SamplingId = NOT_SAMPLING;
	m_SamplingName = "";
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void CaptureInstance::SendToTelemetry()
{
debugLocationMetricsList *pList = GetResults();

	// Send each capture instance to NetworkTelemetry
	for(int i=0;i<pList->results.size();i++)
	{
		metricsList *pMetric = pList->results[i];
#if !__FINAL
		CNetworkDebugTelemetry::AddAutoGPUTelemetry(pMetric);
#endif
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
static void GetBasePath(char *filename, bool useDev = false )
{
	char baseName[RAGE_MAX_PATH];

	// The thing with the __DATE__:
	// MMM DD YYYY
	// 01234567890
	// Apr  1 2011 -> 11Apr01
	formatf(baseName, "autoCapture.%s.%c%c%c%c%c%c%c", 
		__XENON == 1 ? "360" : (__PS3 == 1 ? "ps3" : "xxx"),
		__DATE__[9],__DATE__[10],
		__DATE__[0],__DATE__[1],__DATE__[2],
		__DATE__[4] == ' ' ? '0' : __DATE__[4], __DATE__[5]
	);

	if ( useDev )
	{
		snprintf(filename, RAGE_MAX_PATH, "devkit:\\%s", baseName);
	}
	else
	{
		sprintf(filename, "%s%s", CFileMgr::GetRootDirectory(),baseName);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
#if DEBUG_METRICS_CAPTURES
static const float g_AutoCaptureWarpTime = 5.0f * 1000.0f;
static const float g_AutoCaptureCallbackTime = 2.5f * 1000.0f;
static const float g_AutoCaptureTimeBeforeStart = 5.0f * 1000.0f;
#else
static const float g_AutoCaptureWarpTime = 25.0f * 1000.0f;
static const float g_AutoCaptureCallbackTime = 15.0f * 1000.0f;
static const float g_AutoCaptureTimeBeforeStart = 10.0f * 1000.0f;
#endif
////////////////////////////////////////////////////////////////////////////////////////////////////

static TimingArray s_Timebars;
static ThreadTimingArray s_Threads;

////////////////////////////////////////////////////////////////////////////////////////////////////
debugLocationList *g_LocationList;
static debugLocationList::visitCallback g_AutoCaptureCallback;
static bool g_AutoCaptureRun = false;
static float g_AutoCaptureStartTime = 0.0f;
static float g_AutoCaptureLastWarningTime = 0.0f;
static bool g_PlayerCanBeDamagedBaseState = false;
static CaptureInstance g_AutoCapture;

////////////////////////////////////////////////////////////////////////////////////////////////////
static CaptureInstance g_OnDemandCapture;
static CMetricsCaptureWindow g_OnDemandWindow(*g_OnDemandCapture.GetResults());
static bool g_OnDemandShowWindow = false;

////////////////////////////////////////////////////////////////////////////////////////////////////
CaptureInstance g_ZoneCapture;
static CMetricsCaptureWindow g_ZoneWindow(*g_ZoneCapture.GetResults());
static bool g_ZoneShowWindow = false;

////////////////////////////////////////////////////////////////////////////////////////////////////
static CaptureInstance s_SmokeTestCapture;
static CMetricsCaptureWindow g_SmokeTestWindow(*s_SmokeTestCapture.GetResults());
static bool g_SmokeTestShowWindow = false;

static SampledValue<float> s_SmokeTestFramesPerSecond;
static SampledValue<float> s_SmokeTestMSPerFrame;

static MemoryBucketUsage s_SmokeTestMemoryBucketUsages[] = 
{
	{MEMBUCKET_DEFAULT},
	{MEMBUCKET_ANIMATION},
	{MEMBUCKET_STREAMING},
	{MEMBUCKET_WORLD},
	{MEMBUCKET_GAMEPLAY},
	{MEMBUCKET_FX},
	{MEMBUCKET_RENDER},
	{MEMBUCKET_PHYSICS},
	{MEMBUCKET_AUDIO},
	{MEMBUCKET_NETWORK},
	{MEMBUCKET_SYSTEM},
	{MEMBUCKET_SCALEFORM},
	{MEMBUCKET_SCRIPT},
	{MEMBUCKET_RESOURCE},
	{MEMBUCKET_UI},
	{MEMBUCKET_DEBUG},
	{(MemoryBucketIds)MEMBUCKET_SLOSH},			//slosh goes here
	{MEMBUCKET_INVALID}
};

static memoryHeapResult s_SmokeTestMemoryHeaps[] = 
{
	{MEMHEAP_GAME_VIRTUAL},
	{MEMHEAP_RESOURCE_VIRTUAL},
	{MEMHEAP_DEBUG_VIRTUAL},
	{MEMHEAP_STREAMING_VIRTUAL},
	{MEMHEAP_EXTERNAL_VIRTUAL},
	{MEMHEAP_SLOSH_VIRTUAL},
	{MEMHEAP_SMALL_ALLOCATOR},
	{MEMHEAP_INVALID}
};

static DrawListCount s_SmokeTestDrawListCounts[DL_MAX_TYPES];

static ScriptMemoryUsage s_SmokeTestScriptMemUsage;

static MemoryDistributionArray s_SmokeTestMemoryDistributionUsage;

static pedAndVehicleStatUsage s_SmokeTestPedAndVehicleStatUsage;

static StreamingStatsArray g_SmokeTestStreamingStats;

////////////////////////////////////////////////////////////////////////////////////////////////////
static SampledValue<float> g_AutoFramesPerSecond;
static SampledValue<float> g_AutoMSPerFrame;

static TimingArray g_AutoCpuTimings;
static ThreadTimingArray g_AutoThreadTimings;

static MemoryBucketUsage g_AutoMemoryBucketUsages[] = 
{
	{MEMBUCKET_DEFAULT},
	{MEMBUCKET_ANIMATION},
	{MEMBUCKET_STREAMING},
	{MEMBUCKET_WORLD},
	{MEMBUCKET_GAMEPLAY},
	{MEMBUCKET_FX},
	{MEMBUCKET_RENDER},
	{MEMBUCKET_PHYSICS},
	{MEMBUCKET_AUDIO},
	{MEMBUCKET_NETWORK},
	{MEMBUCKET_SYSTEM},
	{MEMBUCKET_SCALEFORM},
	{MEMBUCKET_SCRIPT},
	{MEMBUCKET_RESOURCE},
	{MEMBUCKET_DEBUG},
	{MEMBUCKET_INVALID}
};

static DrawListCount g_AutoDrawListCounts[DL_MAX_TYPES];

static ScriptMemoryUsage g_AutoScriptMemUsage;

static MemoryDistributionArray g_AutoMemoryDistributionUsage;

static pedAndVehicleStatUsage g_AutoPedAndVehicleStatUsage;

static StreamingStatsArray g_AutoStreamingStats;

////////////////////////////////////////////////////////////////////////////////////////////////////
static SampledValue<float> g_OnDemandFramesPerSecond;
static SampledValue<float> g_OnDemandMSPerFrame;

static TimingArray g_OnDemandCpuTimings;
static ThreadTimingArray g_OnDemandThreadTimings;
static MemoryBucketUsage g_OnDemandMemoryBucketUsages[] = 
{
	{MEMBUCKET_DEFAULT},
	{MEMBUCKET_ANIMATION},
	{MEMBUCKET_STREAMING},
	{MEMBUCKET_WORLD},
	{MEMBUCKET_GAMEPLAY},
	{MEMBUCKET_FX},
	{MEMBUCKET_RENDER},
	{MEMBUCKET_PHYSICS},
	{MEMBUCKET_AUDIO},
	{MEMBUCKET_NETWORK},
	{MEMBUCKET_SYSTEM},
	{MEMBUCKET_SCALEFORM},
	{MEMBUCKET_SCRIPT},
	{MEMBUCKET_RESOURCE},
	{MEMBUCKET_DEBUG},
	{MEMBUCKET_INVALID}
};

static DrawListCount g_OnDemandDrawListCounts[DL_MAX_TYPES];

static ScriptMemoryUsage g_OnDemandScriptMemUsage;

static MemoryDistributionArray g_OnDemandMemoryDistributionUsage;

static pedAndVehicleStatUsage g_OnDemandPedAndVehicleStatUsage;

static StreamingStatsArray g_OnDemandStreamingStats;


////////////////////////////////////////////////////////////////////////////////////////////////////
static SampledValue<float> g_ZoneFramesPerSecond;
static SampledValue<float> g_ZoneMSPerFrame;



static MemoryBucketUsage g_ZoneMemoryBucketUsages[] = 
{
	{MEMBUCKET_DEFAULT},
	{MEMBUCKET_ANIMATION},
	{MEMBUCKET_STREAMING},
	{MEMBUCKET_WORLD},
	{MEMBUCKET_GAMEPLAY},
	{MEMBUCKET_FX},
	{MEMBUCKET_RENDER},
	{MEMBUCKET_PHYSICS},
	{MEMBUCKET_AUDIO},
	{MEMBUCKET_NETWORK},
	{MEMBUCKET_SYSTEM},
	{MEMBUCKET_SCALEFORM},
	{MEMBUCKET_SCRIPT},
	{MEMBUCKET_RESOURCE},
	{MEMBUCKET_UI},
	{MEMBUCKET_DEBUG},
	{MEMBUCKET_INVALID}
};

static DrawListCount g_ZoneDrawListCounts[DL_MAX_TYPES];

static ScriptMemoryUsage g_ZoneScriptMemUsage;

static MemoryDistributionArray g_ZoneMemoryDistributionUsage;

static pedAndVehicleStatUsage g_ZonePedAndVehicleStatUsage;

static StreamingStatsArray g_ZoneStreamingStats;

////////////////////////////////////////////////////////////////////////////////////////////////////
void InitDrawListCounts()
{
	// starts at the first valid renderphase (1)
	for (int i = 1; i < DL_MAX_TYPES; i++)
	{
		g_AutoDrawListCounts[i - 1].id = i;
	}
	g_AutoDrawListCounts[DL_MAX_TYPES - 1].id = -1;

	// starts at the first valid renderphase (1)
	for (int i = 1; i < DL_MAX_TYPES; i++)
	{
		g_OnDemandDrawListCounts[i - 1].id = i;
	}
	g_OnDemandDrawListCounts[DL_MAX_TYPES - 1].id = -1;

	g_ZoneDrawListCounts[0].id = -1;

	// starts at the first valid renderphase (1)
	for (int i = 1; i < DL_MAX_TYPES; i++)
	{
		s_SmokeTestDrawListCounts[i - 1].id = i;
	}
	s_SmokeTestDrawListCounts[DL_MAX_TYPES - 1].id = -1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void SaveMemVisualizeFiles(const char *RAGE_TRACKING_ONLY(targetFile))
{
#if RAGE_TRACKING
	if (diagTracker::GetCurrent())
	{
		char fullPath[RAGE_MAX_PATH];
		ASSET.FullWritePath(fullPath, sizeof(fullPath), targetFile, "");

		// We'll use the final name as a directory.
		char pathName[RAGE_MAX_PATH];
		ASSET.RemoveExtensionFromPath(pathName, sizeof(pathName), fullPath);

		char finalPath[RAGE_MAX_PATH];
		formatf(finalPath, "%s_MemVisualize/MV.mvz", pathName);
		diagTracker::GetCurrent()->FullReport(finalPath);
	}
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void OverridePathName(char *targetBuffer, size_t targetBufferSize, const char *filename)
{
	const char *pathAddition = NULL;

	if (PARAM_autocapturepath.Get(pathAddition))
	{
		// Remove the existing path information.
		const char *nameOnly = ASSET.FileName(filename);
		char pathOnly[RAGE_MAX_PATH];
		ASSET.RemoveNameFromPath(pathOnly, RAGE_MAX_PATH, filename);

		const char *platform = PLATFORM_STRING;

		formatf(targetBuffer, targetBufferSize, "%s/%s/%s/%s", pathOnly, pathAddition, platform, nameOnly);
	}
	else
	{
		safecpy(targetBuffer, filename, targetBufferSize);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void SaveResults(const debugLocationMetricsList * resultList)
{
	USE_DEBUG_MEMORY();

	Assert(resultList);

	char baseFilename[RAGE_MAX_PATH];
	GetBasePath(baseFilename);

	char filename[RAGE_MAX_PATH];
	OverridePathName(filename, sizeof(filename), baseFilename);

	ASSET.CreateLeadingPath(filename);
	SaveMemVisualizeFiles(filename);

	fiStream* stream = ASSET.Create(filename, "xml");
	if (stream)
	{
		// Use the XML writer instead of PARSER.SaveObject because none of these objects have onSave callbacks that would require us to use SaveObject
		parXmlWriterVisitor visitor(stream);
		visitor.Visit(const_cast<debugLocationMetricsList&>(*resultList));
		stream->Close();
	}
	else
	{
		Errorf("Saving %s.xml failed", filename);
	}

	Displayf("Saved as %s.xml", filename);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void AutoCaptureToggle()
{
    g_AutoCaptureRun = !g_AutoCaptureRun;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void OnDemandCaptureToggle()
{
	USE_DEBUG_MEMORY();

	if(!g_OnDemandCapture.Running())
	{
		g_OnDemandCapture.Reset();
		g_OnDemandCapture.StartCapture();
	}

	if(g_OnDemandCapture.Sampling())
	{
		g_OnDemandCapture.StopSampling();
		StoreFramesPerSecondResult(g_OnDemandCapture.GetResults(), g_OnDemandFramesPerSecond);
		StoreMSPerFrameResult(g_OnDemandCapture.GetResults(), g_OnDemandMSPerFrame);
		StoreCPUTimingResults(g_OnDemandCapture.GetResults(), g_OnDemandCpuTimings);
		StoreThreadTimingResults(g_OnDemandCapture.GetResults(), g_OnDemandThreadTimings);
		StoreMemoryBucketUsageResults(g_OnDemandCapture.GetResults(), g_OnDemandMemoryBucketUsages);
		StoreDrawListResults(g_OnDemandCapture.GetResults(), g_OnDemandDrawListCounts);
		StoreScriptMemResult(g_OnDemandCapture.GetResults(), g_OnDemandScriptMemUsage );
		StoreMemDistributionResult(g_OnDemandCapture.GetResults(), g_OnDemandMemoryDistributionUsage);
		StorePedAndVehicleStatResult(g_OnDemandCapture.GetResults(), g_OnDemandPedAndVehicleStatUsage);
		StoreStreamingStats(g_OnDemandCapture.GetResults(), g_OnDemandStreamingStats);
		SaveResults(g_OnDemandCapture.GetResults());
	}
	else
	{
		InitFramesPerSecondResult(&g_OnDemandFramesPerSecond);
		InitMSPerFrameResult(&g_OnDemandMSPerFrame);
		InitCPUTimingResults(g_OnDemandCpuTimings);
		InitThreadTimingResults(g_OnDemandThreadTimings);
		InitMemoryBucketUsageResults(g_OnDemandMemoryBucketUsages);
		InitDrawListResults(g_OnDemandDrawListCounts);
		InitScriptMemResult(g_OnDemandScriptMemUsage);
		InitMemDistributionResult(g_OnDemandMemoryDistributionUsage);
		InitPedAndVehicleStatResult(g_OnDemandPedAndVehicleStatUsage);
		InitStreamingStats(g_OnDemandStreamingStats);

		g_OnDemandCapture.StartSampling(0, "On Demand Capture");

		for(int i=0;i<gDrawListMgr->GetDrawListTypeCount();i++)
		{
			gpuTimingResult *result = rage_new gpuTimingResult;
			result->idx = i;
			result->name = gDrawListMgr->GetDrawListName(i);
			result->time = gDrawListMgr->GetDrawListGPUTime(i);
			g_OnDemandCapture.GetResults()->results.Top()->gpuResults.Grow() = result;
		}

#if __BANK
		CStreamGraph streamingMemTracker;
		streamingMemTracker.IncludeVirtualMemory(true);
		streamingMemTracker.IncludePhysicalMemory(true);
		streamingMemTracker.IncludeReferencedObjects(true);
		streamingMemTracker.IncludeDontDeleteObjects(true);
		streamingMemTracker.IncludeCachedObjects(true);
		streamingMemTracker.CollectResultsForAutoCapture(g_OnDemandCapture.GetResults()->results.Top()->streamingMemoryResults);
#endif //__BANK
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void InitPlayerState()
{
    CPed *player = CGameWorld::FindLocalPlayer();

    if(player)
    {
        g_PlayerCanBeDamagedBaseState = player->GetPlayerInfo()->GetPlayerDataCanBeDamaged() != 0;
        player->GetPlayerInfo()->SetPlayerCanBeDamaged(false);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void RestorePlayerState()
{
    CPed *player = CGameWorld::FindLocalPlayer();

    if(player)
    {
        player->GetPlayerInfo()->SetPlayerCanBeDamaged(g_PlayerCanBeDamagedBaseState);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void PrintStatus(const char * status)
{
    bool output = diagChannel::GetOutput();
    diagChannel::SetOutput(true);
    Displayf("%s", status);
    diagChannel::SetOutput(output);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void StartSingleFrameReport()
{
    OnDemandCaptureToggle();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void StartMultiFrameReport()
{
    OnDemandCaptureToggle();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void ProcessParams()
{
    // allow the capture to be started by a param the first time through
    static bool paramStartCapture = PARAM_startAutoMetricsCapture.Get() || PARAM_startAutoGPUCapture.Get();
    if(paramStartCapture)
    {
        paramStartCapture = false;
        AutoCaptureToggle();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void ProcessKeyboardInput()
{
    if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_F12, KEYBOARD_MODE_DEBUG, "metrics report immediate"))
    {
        StartSingleFrameReport();
    }
    if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_F12, KEYBOARD_MODE_DEBUG_SHIFT, "metrics report 10s capture"))
    {
        StartMultiFrameReport();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MetricsZonesClear()
{
	if( Verifyf( g_ZoneCapture.Sampling() == false, "Called METRICS_ZONES_CLEAR while sampling is running! Call METRICS_ZONE_STOP first!!\n" ))
	{
		MutexHelper Helper;

		g_ZoneCapture.Reset();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MetricsZonesDisplay(bool show)
{
	g_ZoneShowWindow = show;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MetricsZonesDraw()
{	
	g_ZoneWindow.Draw();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MetricsZonesSave()
{
	SaveResults(g_ZoneCapture.GetResults());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MetricsZonesSaveToFilename(const char *pFilename)
{
	USE_DEBUG_MEMORY();

	Assertf(pFilename != NULL, "Passed filename is NULL");

	char filename[RAGE_MAX_PATH];

#if __WIN32PC
	if (PARAM_fpsCaptureUseUserFolder.Get())
	{
		const char* extraPath = NULL;
		if (PARAM_fpsCaptureExtraPath.Get(extraPath))
		{
			sprintf(filename, "%s%s%s", CFileMgr::GetUserDataRootDirectory(), extraPath, pFilename);
		}
		else
		{
			sprintf(filename, "%s%s", CFileMgr::GetUserDataRootDirectory(), pFilename);
		}
	}
	else
#endif
	{
		char rawFilename[RAGE_MAX_PATH];
#if RSG_DURANGO && !__FINAL
		formatf(rawFilename, "debug:/%s", pFilename);
#else
		formatf(rawFilename, "%s%s", CFileMgr::GetRootDirectory(), pFilename);
#endif // RSG_DURANGO && !__FINAL

		OverridePathName(filename, sizeof(filename), rawFilename);
	}

	ASSET.CreateLeadingPath(filename);
	SaveMemVisualizeFiles(filename);

	Verifyf(PARSER.SaveObject(filename, "xml", g_ZoneCapture.GetResults(), parManager::XML), "Saving %s.xml failed", filename);
	Displayf("Saved zone report as %s.xml", filename);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MetricsZoneStart(const char * zoneName)
{
	if(!g_ZoneCapture.Running())
	{
		g_ZoneCapture.StartCapture();
	}
	if(g_ZoneCapture.Sampling())
	{
		MetricsZoneStop();
	}
	InitFramesPerSecondResult(&g_ZoneFramesPerSecond);
	InitMSPerFrameResult(&g_ZoneMSPerFrame);
	InitCPUTimingResults(s_Timebars);
	InitThreadTimingResults(s_Threads);
	InitMemoryBucketUsageResults(g_ZoneMemoryBucketUsages);
	InitDrawListResults(g_ZoneDrawListCounts);
	InitScriptMemResult(g_ZoneScriptMemUsage);
	InitMemDistributionResult(g_ZoneMemoryDistributionUsage);
	InitPedAndVehicleStatResult(g_ZonePedAndVehicleStatUsage);
	InitStreamingStats(g_ZoneStreamingStats);
	g_ZoneCapture.StartSampling(0, zoneName);

}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MetricsZoneStop()
{
	Assertf(g_ZoneCapture.GetResults()->results.GetCount(), "Calling MetricsZoneStop() on an empty result list - was MetricsZoneStart() not called first?");

	g_ZoneCapture.StopSampling();
	StoreFramesPerSecondResult(g_ZoneCapture.GetResults(), g_ZoneFramesPerSecond);
	StoreMSPerFrameResult(g_ZoneCapture.GetResults(), g_ZoneMSPerFrame);
	StoreCPUTimingResults(g_ZoneCapture.GetResults(), s_Timebars);
	StoreThreadTimingResults(g_ZoneCapture.GetResults(), s_Threads);
	StoreMemoryBucketUsageResults(g_ZoneCapture.GetResults(), g_ZoneMemoryBucketUsages);
	StoreDrawListResults(g_ZoneCapture.GetResults(), g_ZoneDrawListCounts);
	StoreScriptMemResult(g_ZoneCapture.GetResults(), g_ZoneScriptMemUsage);
	StoreMemDistributionResult(g_ZoneCapture.GetResults(), g_ZoneMemoryDistributionUsage);
	StorePedAndVehicleStatResult(g_ZoneCapture.GetResults(), g_ZonePedAndVehicleStatUsage);
	StoreStreamingStats(g_ZoneCapture.GetResults(), g_ZoneStreamingStats);
}

// DEBUG
void MetricsZoneWriteTelemetry()
{
	g_ZoneCapture.SendToTelemetry();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MetricsSmokeTestClear()
{
	if( Verifyf( g_ZoneCapture.Sampling() == false, "Called METRICS_ZONES_CLEAR while sampling is running! Call METRICS_ZONE_STOP first!!\n" ))
	{
		MutexHelper Helper;

		s_SmokeTestCapture.Reset();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MetricsSmokeTestDisplay(bool show)
{
	g_SmokeTestShowWindow = show;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MetricsSmokeTestDraw()
{	
	g_SmokeTestWindow.Draw();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MetricsSmokeTestSave()
{
	SaveResults(s_SmokeTestCapture.GetResults());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MetricsSmokeTestSaveToFilename(const char *pFilename)
{
	USE_DEBUG_MEMORY();

	Assertf(pFilename != NULL, "Passed filename is NULL");

	char rawFilename[RAGE_MAX_PATH];
	formatf(rawFilename, "%s%s", CFileMgr::GetRootDirectory(), pFilename);

	char filename[RAGE_MAX_PATH];
	OverridePathName(filename, sizeof(filename), rawFilename);

	ASSET.CreateLeadingPath(filename);
	SaveMemVisualizeFiles(filename);

	Verifyf(PARSER.SaveObject(filename, "xml", s_SmokeTestCapture.GetResults(), parManager::XML), "Saving %s.xml failed", filename);
	Displayf("Saved zone report as %s.xml", filename);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
void MetricsSmokeTestStop()
{
	Assertf(s_SmokeTestCapture.GetResults()->results.GetCount(), "Calling MetricsSmokeTestStop() on an empty result list - was MetricsSmokeTestStart() not called first?");

	s_SmokeTestCapture.StopSampling();
	StoreFramesPerSecondResult(s_SmokeTestCapture.GetResults(), s_SmokeTestFramesPerSecond);
	StoreMSPerFrameResult(s_SmokeTestCapture.GetResults(), s_SmokeTestMSPerFrame);
	StoreCPUTimingResults(s_SmokeTestCapture.GetResults(), s_Timebars);
	StoreMemoryBucketUsageResults(s_SmokeTestCapture.GetResults(), s_SmokeTestMemoryBucketUsages);
	StoreMemHeapResults(s_SmokeTestCapture.GetResults());
	StoreStreamingModuleMemory(s_SmokeTestCapture.GetResults());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MetricsSmokeTestStart(const char * zoneName)
{
	if(!s_SmokeTestCapture.Running())
	{
		s_SmokeTestCapture.StartCapture();
	}
	if(s_SmokeTestCapture.Sampling())
	{
		MetricsSmokeTestStop();
	}

	InitFramesPerSecondResult(&s_SmokeTestFramesPerSecond);
	InitMSPerFrameResult(&s_SmokeTestMSPerFrame);
	InitCPUTimingResults(s_Timebars);
	InitMemoryBucketUsageResults(s_SmokeTestMemoryBucketUsages);
	s_SmokeTestCapture.StartSampling(0, zoneName);

}



////////////////////////////////////////////////////////////////////////////////////////////////////
#if __BANK
void AddWidgets(bkBank& bank)
{
	bank.AddToggle("Show On Demand Window", &g_OnDemandShowWindow);
	bank.AddToggle("Show Zone Window", &g_ZoneShowWindow);
	bank.AddButton("Start/Stop Auto Metrics Capture",CFA(AutoCaptureToggle));
	bank.AddButton("Start/Stop On Demand Metrics Capture",CFA(OnDemandCaptureToggle));
	bank.AddButton("Save zone report",CFA(MetricsZonesSave));
	bank.AddButton("Printout Script Timing Report",CFA(SetScriptReportAllScriptTimes));

	// DEBUG, to test telemetry writing for Zone Capture Report
	bank.AddButton("Metrics Zone Start",CFA(MetricsZoneStart));
	bank.AddButton("Metrics Zone Stop",CFA(MetricsZoneStop));
	bank.AddButton("Metrics Zone Write Telemetry",CFA(MetricsZoneWriteTelemetry));
}
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
bool AutoCaptureCountdownRunning(float currentTime)
{
	return g_AutoCaptureStartTime != 0.0f && (currentTime < g_AutoCaptureStartTime);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void AutoCaptureCountdownUpdate(float currentTime)
{
	float deltaTime = (currentTime - g_AutoCaptureLastWarningTime);
	if(deltaTime > 1000.0f)
	{
		bool output = diagChannel::GetOutput();
		diagChannel::SetOutput(true);
		Displayf("About to start auto metrics capture, %.0f", (g_AutoCaptureStartTime - currentTime) / 1000.0f);
		diagChannel::SetOutput(output);
		g_AutoCaptureLastWarningTime = currentTime;
	} 
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool AutoCaptureCountdownExpired(float currentTime)
{
	return g_AutoCaptureStartTime != 0.0f && (currentTime > g_AutoCaptureStartTime);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void AutoCaptureCountdownStart(float currentTime, float countdownMilliseconds)
{
	g_AutoCaptureStartTime = currentTime + countdownMilliseconds;
	g_AutoCaptureLastWarningTime = currentTime;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void AutoCaptureCountdownCancel()
{
	g_AutoCaptureStartTime = 0.0f;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool AutoCaptureCallback(int currentLocation)
{
	USE_DEBUG_MEMORY();

	char name[RAGE_MAX_PATH];
	char nameScreenshot[RAGE_MAX_PATH];
	char capturePath[RAGE_MAX_PATH];
	char screenshotPath[RAGE_MAX_PATH];
	{
        MutexHelper Helper;

		GetBasePath(name, __XENON);
		GetBasePath(nameScreenshot);
	
	
		InitFramesPerSecondResult(&g_AutoFramesPerSecond);
		InitMSPerFrameResult(&g_AutoMSPerFrame);
		InitCPUTimingResults(g_AutoCpuTimings);
		InitThreadTimingResults(g_AutoThreadTimings);
		InitMemoryBucketUsageResults(g_AutoMemoryBucketUsages);
		InitDrawListResults(g_AutoDrawListCounts);
		InitScriptMemResult(g_AutoScriptMemUsage);
		InitMemDistributionResult(g_AutoMemoryDistributionUsage);
		InitPedAndVehicleStatResult(g_AutoPedAndVehicleStatUsage);
		InitStreamingStats(g_AutoStreamingStats);

		g_AutoCapture.StartSampling(currentLocation, g_LocationList->GetLocationName(currentLocation));

		//If a name is specified use it 
		if (g_LocationList->GetLocationName(currentLocation))
		{
#if __XENON
			formatf(capturePath, "%s.%s.pix2", name, g_LocationList->GetLocationName(currentLocation));
#else
		    formatf(capturePath, "%s.%s.vr", name, g_LocationList->GetLocationName(currentLocation));		
#endif
			formatf(screenshotPath, "%s.%s", nameScreenshot, g_LocationList->GetLocationName(currentLocation));
       
		}
		else
		{
			#if __XENON
				formatf(capturePath, "%s.%d.pix2", name, currentLocation);
			#else
				formatf(capturePath, "%s.%d.vr", name, currentLocation);	
			#endif	
				formatf(screenshotPath, "%s.%d", nameScreenshot, currentLocation);
		}

		for(int i=0;i<gDrawListMgr->GetDrawListTypeCount();i++)
		{
			gpuTimingResult *result = rage_new gpuTimingResult;
			result->idx = i;
			result->name = gDrawListMgr->GetDrawListName(i);
			result->time = gDrawListMgr->GetDrawListGPUTime(i);
			g_AutoCapture.GetResults()->results.Top()->gpuResults.Grow() = result;
		}


#if __BANK
		CStreamGraph streamingMemTracker;
		streamingMemTracker.IncludeVirtualMemory(true);
		streamingMemTracker.IncludePhysicalMemory(true);
		streamingMemTracker.IncludeReferencedObjects(true);
		streamingMemTracker.IncludeDontDeleteObjects(true);
		streamingMemTracker.IncludeCachedObjects(true);
		streamingMemTracker.CollectResultsForAutoCapture(g_AutoCapture.GetResults()->results.Top()->streamingMemoryResults);
#endif //__BANK
	}


#if DEBUG_METRICS_CAPTURES == 0 
	CSystem::GetRageSetup()->SetScreenShotNamingConvention( grcSetup::OVERWRITE_SCREENSHOT );
	CSystem::GetRageSetup()->TakeScreenShot( screenshotPath);
	Assert( !PIXIsGPUCapturing() );
	PIXSaveGPUCapture(capturePath);
#endif // DEBUG_METRICS_CAPTURES == 0 


	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void StartAutoCapture()
{
	g_AutoCaptureCallback.Bind(&AutoCaptureCallback);
	g_LocationList->SetVisitCallback(g_AutoCaptureCallback);
	g_AutoCapture.StartCapture();
	g_LocationList->Visit(g_AutoCaptureWarpTime, g_AutoCaptureCallbackTime);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void StopAutoCapture()
{
	g_LocationList->StopVisiting();
	g_AutoCapture.Reset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// SmokeTest

//static const char* s_lastSection = NULL;

void SmokeTestSectionCaptureStart(const char * section)
{
	USE_DEBUG_MEMORY();

	if(!s_SmokeTestCapture.Running())
	{
		s_SmokeTestCapture.StartCapture();
	}
//	s_lastSection = section;

	Assert(!s_SmokeTestCapture.Sampling());

	InitFramesPerSecondResult(&s_SmokeTestFramesPerSecond);
	InitMSPerFrameResult(&s_SmokeTestMSPerFrame);
	InitCPUTimingResults(s_Timebars);
	InitThreadTimingResults(s_Threads);
	InitMemoryBucketUsageResults(s_SmokeTestMemoryBucketUsages);
	InitDrawListResults(s_SmokeTestDrawListCounts);
	InitScriptMemResult(s_SmokeTestScriptMemUsage);
	InitMemDistributionResult(s_SmokeTestMemoryDistributionUsage);
	InitPedAndVehicleStatResult(s_SmokeTestPedAndVehicleStatUsage);
	InitStreamingStats(g_SmokeTestStreamingStats);

	s_SmokeTestCapture.StartSampling(s_SmokeTestCapture.GetResults()->results.GetCount(), section);

	for(int i=0;i<gDrawListMgr->GetDrawListTypeCount();i++)
	{
		gpuTimingResult *result = rage_new gpuTimingResult;
		result->idx = i;
		result->name = gDrawListMgr->GetDrawListName(i);
		result->time = gDrawListMgr->GetDrawListGPUTime(i);
		s_SmokeTestCapture.GetResults()->results.Top()->gpuResults.Grow() = result;
	}

#if __BANK
	CStreamGraph streamingMemTracker;
	streamingMemTracker.IncludeVirtualMemory(true);
	streamingMemTracker.IncludePhysicalMemory(true);
	streamingMemTracker.IncludeReferencedObjects(true);
	streamingMemTracker.IncludeDontDeleteObjects(true);
	streamingMemTracker.IncludeCachedObjects(true);
	streamingMemTracker.CollectResultsForAutoCapture(s_SmokeTestCapture.GetResults()->results.Top()->streamingMemoryResults);
#endif //__BANK
}

void SmokeTestSectionCaptureStop()
{
	Assert(s_SmokeTestCapture.Sampling());
	s_SmokeTestCapture.StopSampling();
	StoreFramesPerSecondResult(s_SmokeTestCapture.GetResults(), s_SmokeTestFramesPerSecond);
	StoreMSPerFrameResult(s_SmokeTestCapture.GetResults(), s_SmokeTestMSPerFrame);
	StoreCPUTimingResults(s_SmokeTestCapture.GetResults(), s_Timebars);
	StoreThreadTimingResults(s_SmokeTestCapture.GetResults(), s_Threads);
	StoreMemoryBucketUsageResults(s_SmokeTestCapture.GetResults(), s_SmokeTestMemoryBucketUsages);
	StoreMemHeapResults(s_SmokeTestCapture.GetResults());
	StoreDrawListResults(s_SmokeTestCapture.GetResults(), s_SmokeTestDrawListCounts);
	StoreScriptMemResult(s_SmokeTestCapture.GetResults(), s_SmokeTestScriptMemUsage);
	StoreMemDistributionResult(s_SmokeTestCapture.GetResults(), s_SmokeTestMemoryDistributionUsage);
	StorePedAndVehicleStatResult(s_SmokeTestCapture.GetResults(), s_SmokeTestPedAndVehicleStatUsage);
	StoreStreamingStats(g_AutoCapture.GetResults(), g_SmokeTestStreamingStats);
}

void SmokeTestSaveResults(const char * report)
{
	USE_DEBUG_MEMORY();

	char filename[256];
	formatf(filename, "%s%s", CFileMgr::GetRootDirectory(), report);

	ASSET.CreateLeadingPath(filename);
	SaveMemVisualizeFiles(filename);

	Verifyf(PARSER.SaveObject(report, "xml", s_SmokeTestCapture.GetResults(), parManager::XML), "Saving %s.xml failed", report);
	Displayf("Saved report as %s.xml", report);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
static void UpdateSmokeTestCapture(float UNUSED_PARAM(currentTime))
{
	if(s_SmokeTestCapture.Running())
	{
		if(s_SmokeTestCapture.Sampling())
		{
			UpdateFramesPerSecondResult(&s_SmokeTestFramesPerSecond);
			UpdateMSPerFrameResult(&s_SmokeTestMSPerFrame);
			UpdateCPUTimingResults(s_Timebars);            
			UpdateThreadTimingResults(s_Threads);
			UpdateMemoryBucketUsageResults(s_SmokeTestMemoryBucketUsages);
			UpdateDrawListResults(s_SmokeTestDrawListCounts);
			UpdateScriptMemResult(s_SmokeTestScriptMemUsage);
			UpdateMemDistributionResult(s_SmokeTestMemoryDistributionUsage);
			UpdatePedAndVehicleStatResult(s_SmokeTestPedAndVehicleStatUsage);
			UpdateStreamingStats(g_SmokeTestStreamingStats);
		}
	}
}

static void UpdateAutomatedSmokeTestCapture(float UNUSED_PARAM(currentTime))
{
	if(s_SmokeTestCapture.Running())
	{
		if(s_SmokeTestCapture.Sampling())
		{
			UpdateFramesPerSecondResult(&s_SmokeTestFramesPerSecond);
			UpdateMSPerFrameResult(&s_SmokeTestMSPerFrame);	
			UpdateCPUTimingResults(s_Timebars);          
			UpdateMemoryBucketUsageResults(s_SmokeTestMemoryBucketUsages);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UpdateAutoCapture(float currentTime)
{
	if(g_AutoCaptureRun)
	{
		if(g_AutoCapture.Running())
		{
			if(g_LocationList->Visiting())
			{
				if(g_AutoCapture.Sampling() && g_LocationList->GetCurrentLocation() != g_AutoCapture.GetSamplingId())
				{
					g_AutoCapture.StopSampling();
					StoreFramesPerSecondResult(g_AutoCapture.GetResults(), g_AutoFramesPerSecond);
					StoreMSPerFrameResult(g_AutoCapture.GetResults(), g_AutoMSPerFrame);
					StoreCPUTimingResults(g_AutoCapture.GetResults(), g_AutoCpuTimings);
					StoreThreadTimingResults(g_AutoCapture.GetResults(), g_AutoThreadTimings);
					StoreMemoryBucketUsageResults(g_AutoCapture.GetResults(), g_AutoMemoryBucketUsages);
					StoreDrawListResults(g_AutoCapture.GetResults(), g_AutoDrawListCounts);
					StoreScriptMemResult(g_AutoCapture.GetResults(), g_AutoScriptMemUsage);
					StoreMemDistributionResult(g_AutoCapture.GetResults(), g_AutoMemoryDistributionUsage);
					StorePedAndVehicleStatResult(g_AutoCapture.GetResults(), g_AutoPedAndVehicleStatUsage);
					StoreStreamingStats(g_AutoCapture.GetResults(), g_AutoStreamingStats);
				}
				else if(g_AutoCapture.Sampling())
				{
					UpdateFramesPerSecondResult(&g_AutoFramesPerSecond);
					UpdateMSPerFrameResult(&g_AutoMSPerFrame);
					UpdateCPUTimingResults(g_AutoCpuTimings);            
					UpdateThreadTimingResults(g_AutoThreadTimings);            
					UpdateMemoryBucketUsageResults(g_AutoMemoryBucketUsages);
					UpdateDrawListResults(g_AutoDrawListCounts);
					UpdateScriptMemResult(g_AutoScriptMemUsage);
					UpdateMemDistributionResult(g_AutoMemoryDistributionUsage);
					UpdatePedAndVehicleStatResult(g_AutoPedAndVehicleStatUsage);
					UpdateStreamingStats(g_AutoStreamingStats);
				}
			}
			else
			{
				g_AutoCapture.StopSampling();
				StoreFramesPerSecondResult(g_AutoCapture.GetResults(), g_AutoFramesPerSecond);
				StoreMSPerFrameResult(g_AutoCapture.GetResults(), g_AutoMSPerFrame);
				StoreCPUTimingResults(g_AutoCapture.GetResults(), g_AutoCpuTimings);
				StoreThreadTimingResults(g_AutoCapture.GetResults(), g_AutoThreadTimings);
				StoreMemoryBucketUsageResults(g_AutoCapture.GetResults(), g_AutoMemoryBucketUsages);
				StoreDrawListResults(g_AutoCapture.GetResults(), g_AutoDrawListCounts);
				StoreScriptMemResult(g_AutoCapture.GetResults(), g_AutoScriptMemUsage);
				StoreMemDistributionResult(g_AutoCapture.GetResults(), g_AutoMemoryDistributionUsage);
				StorePedAndVehicleStatResult(g_AutoCapture.GetResults(), g_AutoPedAndVehicleStatUsage);
				StoreStreamingStats(g_AutoCapture.GetResults(), g_AutoStreamingStats);

				SaveResults(g_AutoCapture.GetResults());
				AutoCaptureToggle();
			}
		}
		else if(AutoCaptureCountdownRunning(currentTime))
		{
			AutoCaptureCountdownUpdate(currentTime);
		}
		else if(AutoCaptureCountdownExpired(currentTime))
		{
			PrintStatus("Metrics capture started");	
			InitPlayerState();
			StartAutoCapture();
		}
		else
		{
			AutoCaptureCountdownStart(currentTime, g_AutoCaptureTimeBeforeStart);
		}
	}
	else
	{
		if(g_AutoCapture.Running())
		{
			PrintStatus("Metrics capture stopped");
			StopAutoCapture();
			RestorePlayerState();
		}
		AutoCaptureCountdownCancel();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UpdateOnDemandCapture(float UNUSED_PARAM(currentTime))
{
	if(g_OnDemandCapture.Running())
	{
		if(g_OnDemandCapture.Sampling())
		{
			UpdateFramesPerSecondResult(&g_OnDemandFramesPerSecond);
			UpdateMSPerFrameResult(&g_OnDemandMSPerFrame);
			UpdateCPUTimingResults(g_OnDemandCpuTimings);            
			UpdateThreadTimingResults(g_OnDemandThreadTimings);            
			UpdateMemoryBucketUsageResults(g_OnDemandMemoryBucketUsages);
			UpdateDrawListResults(g_OnDemandDrawListCounts);
			UpdateScriptMemResult(g_OnDemandScriptMemUsage);
			UpdateMemDistributionResult(g_OnDemandMemoryDistributionUsage);
			UpdatePedAndVehicleStatResult(g_OnDemandPedAndVehicleStatUsage);
			UpdateStreamingStats(g_OnDemandStreamingStats);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void UpdateZoneCapture(float UNUSED_PARAM(currentTime))
{
	if(g_ZoneCapture.Running())
	{
		if(g_ZoneCapture.Sampling())
		{
			UpdateFramesPerSecondResult(&g_ZoneFramesPerSecond);
			UpdateMSPerFrameResult(&g_ZoneMSPerFrame);
			UpdateCPUTimingResults(s_Timebars);            
			UpdateThreadTimingResults(s_Threads);            
			UpdateMemoryBucketUsageResults(g_ZoneMemoryBucketUsages);
			UpdateDrawListResults(g_ZoneDrawListCounts);
			UpdateScriptMemResult(g_ZoneScriptMemUsage);
			UpdateMemDistributionResult(g_ZoneMemoryDistributionUsage);
			UpdatePedAndVehicleStatResult(g_ZonePedAndVehicleStatUsage);
			UpdateStreamingStats(g_ZoneStreamingStats);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Init()
{
	ProcessParams();
	InitDrawListCounts();
	g_OnDemandWindow.Move(0.5f, 0.2f);

	MutexHelper::sm_Mutex = rage::sysIpcCreateMutex();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Update(float currentTime)
{
    MutexHelper Helper;

    ProcessKeyboardInput();
	UpdateAutoCapture(currentTime);
	UpdateOnDemandCapture(currentTime);
	UpdateZoneCapture(currentTime);
	UpdateSmokeTestCapture(currentTime);
	UpdateAutomatedSmokeTestCapture(currentTime);

	ScriptTimingUpdate(currentTime);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Shutdown()
{
    sysIpcDeleteMutex(MutexHelper::sm_Mutex);

	g_AutoCapture.Reset();
	g_OnDemandCapture.Reset();
	g_ZoneCapture.Reset();
	s_SmokeTestCapture.Reset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void DrawWindows()
{	
	MutexHelper Helper;

	if(g_OnDemandShowWindow) g_OnDemandWindow.Draw();
	if(g_ZoneShowWindow) g_ZoneWindow.Draw();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Script timing (B*496942)
// Wasn't up to par as it only did all threads, updated to do individual threads also.. now B*529119
#include "script/thread.h"

#if	__WIN32PC
#define SAMPLE_FRAMES_PER_SECOND	65		// PC can go at 60Hz (plus a little overhead)
#else
#define SAMPLE_FRAMES_PER_SECOND	35		// Consoles go at 30Hz (plus a little overhead, looks like it can go faster during loading transitions.)
#endif

// Ring buffer to capture script timing info
#define	SCRIPT_TIME_SAMPLES_TIME (10)
#define NUM_SCRIPT_TIME_SAMPLES	(SAMPLE_FRAMES_PER_SECOND * SCRIPT_TIME_SAMPLES_TIME)		// 30 samples (maxfps) for 10 seconds (upped to 40 to prevent any possible overflow).
#define MIN_SCRIPT_SAMPLE_TIME	( 1000.0f / SAMPLE_FRAMES_PER_SECOND)

class	ScriptTimingRingBufferContents
{
public:
	ScriptTimingRingBufferContents()
	{
		timeStamp = -SCRIPT_TIME_SAMPLES_TIME * 1000.0f;
		cpuTime = 0.0f;
	}
	float	timeStamp;
	float	cpuTime;
};

class ScriptTimingContainer
{
public:
	ScriptTimingContainer()
	{
		m_Name = atHashString("");
		m_ScriptTimingIndex = 0;
		m_DoReport = false;
		m_Updated = false;
	}

	void SetName(const char *pName)
	{
		m_Name = atHashString(pName);
	}

	void SetThreadID(scrThreadId id)
	{
		m_threadID = id;
	}

	u32 GetPreviousTimeIndex()
	{
		s32 lastTimeIndex = m_ScriptTimingIndex-1;
		if( lastTimeIndex < 0)
		{
			lastTimeIndex+=NUM_SCRIPT_TIME_SAMPLES;
		}
		return (u32)lastTimeIndex;
	}

	void GetAverageAndPeakTimes(float currentTime, float &average, float &peak)
	{
		CPUTiming timing;
		timing.value.Reset();
		for(int i=0;i<NUM_SCRIPT_TIME_SAMPLES;i++)
		{
			ScriptTimingRingBufferContents &thisRingEntry = m_ScriptTimings[i];
			if( currentTime - thisRingEntry.timeStamp < (SCRIPT_TIME_SAMPLES_TIME*1000.0f) )
			{
				timing.value.AddSample(thisRingEntry.cpuTime);
			}
		}
		average = timing.value.Mean();
		peak = timing.value.Max();
	}


	atHashString	m_Name;																		// The script name, which is used for the timebar for that script.
	scrThreadId		m_threadID;
	u32				m_ScriptTimingIndex;														// Ring buffer index
	atRangeArray<ScriptTimingRingBufferContents, NUM_SCRIPT_TIME_SAMPLES>	m_ScriptTimings;	// The actual ring buffer
	bool			m_DoReport;
	bool			m_Updated;
};

atArray<ScriptTimingContainer*> g_ScriptTimingContainers;

/*
ScriptTimingContainer *FindTimingContainerByName(const char *pName)
{
	atHashString nameHash(pName);
	for(int i=0;i<g_ScriptTimingContainers.size();i++)
	{
		if( g_ScriptTimingContainers[i]->m_Name == nameHash )
		{
			return g_ScriptTimingContainers[i];
		}
	}
	return NULL;
}
*/

ScriptTimingContainer *FindTimingContainerByThreadID(scrThreadId id)
{
	int numContainers = g_ScriptTimingContainers.size();
	ScriptTimingContainer **pContainerPtr = g_ScriptTimingContainers.begin();
	for(int i=0;i<numContainers;i++)
	{
		ScriptTimingContainer *pContainer = *pContainerPtr;
		if( pContainer->m_threadID == id )
		{
			return pContainer;
		}
		pContainerPtr++;
	}
	return NULL;
}


void UpdateScriptTiming(scrThread *pScriptThread, float currentTime)
{
	USE_DEBUG_MEMORY();

	// Find the existing timing info struct (if it exists)
	ScriptTimingContainer	*pThisTimebarData = FindTimingContainerByThreadID(pScriptThread->GetThreadId());
	// Check if we found it, if not, make one
	if( !pThisTimebarData )
	{
		pThisTimebarData = rage_new ScriptTimingContainer;
		pThisTimebarData->SetName(pScriptThread->GetScriptName());
		pThisTimebarData->SetThreadID(pScriptThread->GetThreadId());
		g_ScriptTimingContainers.Grow() = pThisTimebarData;
	}

	// If we couldn't allocate one, it's busted!
	Assertf(pThisTimebarData!=NULL,"");

	// Check if we're updating faster than NUM_SCRIPT_TIME_SAMPLES(Hz)
	// Get the last time
	u32 lastTimeIndex = pThisTimebarData->GetPreviousTimeIndex();
	ScriptTimingRingBufferContents &lastRingEntry = pThisTimebarData->m_ScriptTimings[lastTimeIndex];
	float diffTime = currentTime - lastRingEntry.timeStamp;
	if( diffTime < MIN_SCRIPT_SAMPLE_TIME )
	{
		// We've got a time update that is faster than the max game update time.. Game is updating faster than 30Hz, skip this sample
		return;
	}

	// Get the next used ring buffer entry
	ScriptTimingRingBufferContents &ringEntry = pThisTimebarData->m_ScriptTimings[pThisTimebarData->m_ScriptTimingIndex];
	// Wrap index
	pThisTimebarData->m_ScriptTimingIndex++;
	if( pThisTimebarData->m_ScriptTimingIndex >= NUM_SCRIPT_TIME_SAMPLES )
	{
		pThisTimebarData->m_ScriptTimingIndex = 0;
	}

	// Assert if the timestamp is not over 10 seconds old (this should now never happen due to overspeed check above)
	Assertf( ringEntry.timeStamp <= (currentTime - (SCRIPT_TIME_SAMPLES_TIME*1000.0f)), "Timestamp is under 10 seconds old!! It's going to go wrong" );

	// Store the timestamp & get the CPU times from the Timebars
	ringEntry.timeStamp = currentTime;

#if SCRIPT_DEBUGGING

	ringEntry.cpuTime = (pScriptThread->GetUpdateTime() - pScriptThread->GetConsoleOutputTime()) * 1000.0f;	// Times in seconds.

#endif	//SCRIPT_DEBUGGING

/*	// If we wanna go back to timebars
#if RAGE_TIMEBARS
	// Using Timebars
	ringEntry.cpuTime = 0.0f;
	int callcount = 0;
	g_pfTB.GetFunctionTotals(pTimebarName, callcount, ringEntry.cpuTime, 1);
#endif	// RAGE_TIMEBARS
*/

	pThisTimebarData->m_Updated = true;
}


void ReportScriptTimings(float currentTime)
{
	int numContainers = g_ScriptTimingContainers.size();
	ScriptTimingContainer **pContainerPtr = g_ScriptTimingContainers.begin();
	for(int i=0;i<numContainers;i++)
	{
		ScriptTimingContainer *pContainer = *pContainerPtr;

		if(pContainer->m_DoReport == true)
		{
			pContainer->m_DoReport = false;

			// Output the data
			float average, peak;
			pContainer->GetAverageAndPeakTimes(currentTime, average, peak);

			u32 lastTimeIndex = pContainer->GetPreviousTimeIndex();
			ScriptTimingRingBufferContents &lastRingEntry = pContainer->m_ScriptTimings[lastTimeIndex];

			// Should now have min/max/mean in timing
			// Should now have this time in the latest lastRingEntry
			Displayf("/-----------------------------------------------------------------------/");
			Displayf("Timing data for script %s:-", pContainer->m_Name.GetCStr() );
			Displayf("Script Current Processing Time = %f (mSecs)", lastRingEntry.cpuTime );
			Displayf("Script Average processing time (over last %d seconds) = %f (mSecs)", SCRIPT_TIME_SAMPLES_TIME, average );
			Displayf("Script Peak processing time (over last %d seconds) = %f (mSecs)", SCRIPT_TIME_SAMPLES_TIME, peak );
			Displayf("/-----------------------------------------------------------------------/");
		}

		// Reset Update Flag here
		pContainer->m_Updated = false;
		pContainerPtr++;
	}
}

// remove anything that wasn't updated this frame.
void CleanupOldScriptTimings()
{
	int numContainers = g_ScriptTimingContainers.size();
	if( numContainers )
	{
		ScriptTimingContainer **pContainerPtr = &g_ScriptTimingContainers[numContainers-1];
		for(int i=numContainers-1;i>=0;i--)
		{
			ScriptTimingContainer *pContainer = *pContainerPtr;
			if(pContainer->m_Updated == false)
			{
				// Ditch it!
				delete pContainer;
				g_ScriptTimingContainers.Delete(i);
			}
			pContainerPtr--;
		}
	}
}

// currentTime == gameTime in milliSeconds (fwTimer::GetTimeInMilliseconds())
void ScriptTimingUpdate(float currentTime)
{

#if !SCRIPT_DEBUGGING
	UNUSED_VAR(currentTime);
#else

	// Each individual running script
	atArray<scrThread*>	&threadsArray = scrThread::GetScriptThreads();
	int numThreads =  threadsArray.size();
	for(int i=0;i<numThreads;i++)
	{
		if(threadsArray[i]->GetThreadId())	// Only process valid threadID's
		{
			UpdateScriptTiming(threadsArray[i], currentTime);	// NOTE: Sets update flag to true for containers that have been updated.
		}
	}

	// Remove script timing containers that are no longer active
	CleanupOldScriptTimings();

	// Report any timings to the TTY
	ReportScriptTimings(currentTime);



#endif

}


void SetScriptReportAllScriptTimes()
{
	for(int i=0;i<g_ScriptTimingContainers.size();i++)
	{
		ScriptTimingContainer *pContainer = g_ScriptTimingContainers[i];
		pContainer->m_DoReport = true;
	}
}


// Stuff for B*554468
float GetScriptUpdateTime(scrThreadId scriptThreadID)
{
	ScriptTimingContainer	*pThisTimebarData = FindTimingContainerByThreadID(scriptThreadID);
	if(pThisTimebarData)
	{
		u32 lastTimeIndex = pThisTimebarData->GetPreviousTimeIndex();
		ScriptTimingRingBufferContents &lastRingEntry = pThisTimebarData->m_ScriptTimings[lastTimeIndex];
		return lastRingEntry.cpuTime;
	}
	return 0.0f;	// error!
}


bool GetScriptPeakAverageTime(scrThreadId scriptThreadID, float &peak, float &average)
{
	ScriptTimingContainer	*pThisTimebarData = FindTimingContainerByThreadID(scriptThreadID);
	if(pThisTimebarData)
	{
		// Since we don't know the current time (this is called via script), just use the last time anything was updated for this script
		// Should be OK since the timing data will have vanished should this script have gone, or updated this frame anyway
		u32 lastTimeIndex = pThisTimebarData->GetPreviousTimeIndex();
		ScriptTimingRingBufferContents &lastRingEntry = pThisTimebarData->m_ScriptTimings[lastTimeIndex];
		// Get average and peak times.
		pThisTimebarData->GetAverageAndPeakTimes(lastRingEntry.timeStamp, average, peak );
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

};		// namespace

#endif	// ENABLE_STATS_CAPTURE
