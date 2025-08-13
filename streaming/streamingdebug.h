//
// streaming/streamingdebug.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef STREAMING_STREAMINGDEBUG_H
#define STREAMING_STREAMINGDEBUG_H

//Rage
#include "atl/array.h"
#include "atl/binmap.h"
#include "atl/hashstring.h"
#include "atl/string.h"
#include "file/limits.h"
#include "parser/macros.h"
#include "vector/color32.h"

//Framework
#include "streaming/streamingdefs.h"

class CEntity;
struct ModuleMemoryStats;

#if __BANK
extern	char s_DumpOutputFile[RAGE_MAX_PATH];
#endif //__BANK

struct MemoryProfileModuleStat : public datBase
{
	s32 m_VirtualMemory;			// Virtual Memory in KB
	s32 m_PhysicalMemory;			// Physical Memory in KB

	MemoryProfileModuleStat()
		: m_VirtualMemory(0)
		, m_PhysicalMemory(0)
	{
	}

	PAR_PARSABLE;
};

struct MemoryFootprint : public datBase
{
	atBinaryMap<MemoryProfileModuleStat, atHashString> m_ModuleMemoryStatsMap;

	PAR_PARSABLE;
};

struct MemoryProfileLocation : public datBase
{
	Vec3V m_PlayerPos;
	Mat34V m_CameraMtx;
	atString m_Name;
	MemoryFootprint m_MemoryFootprint;

	// Meta-locations can't be teleported to, they can just be used as a data source.
	bool IsMetaLocation() const;

	PAR_PARSABLE;
};

struct MemoryProfileLocationList : public datBase
{
	atArray<MemoryProfileLocation> m_Locations;

	void ComputeAverages();

	PAR_PARSABLE;
};

class CStreamingDebug
{
public:
	void Update();

#if __BANK
	static void DisplayResourceHeapPages();
	static void DisplayStolenMemory();
#endif // __BANK
	// const char* GetObjectName(s32 index);
#if __DEV
	void MemoryStatsObjectsInMemory(s32 moduleId, s32 &total, s32 &virt, s32 &phys, s32 &largest);
#endif

#if __BANK
	static void AddFailedStreamingAllocation(strIndex index, bool bFragmented, datResourceMap& map);
	static void UpdateFailedStreamingRequests();
	static void DisplayFailedStreamingRequests();
	static void StorePvsIfNeeded();
#endif

	static void InitDebugSystems();

	static void DumpModuleMemoryComparedToAverage();

#if __BANK
	static void PrintMemoryDistribution(const sysMemDistribution& distribution,  char* header, char* usedMemory, char* freeMemory);
	static const char **GetMemoryProfileLocationNames();
	static int GetMemoryProfileLocationCount();
	static const MemoryProfileLocationList &GetMemoryProfileLocationList();
	static void LoadMemoryProfileLocations();
#endif // __BANK
	static void CalculateRequestedObjectSizes(s32& virtualSize, s32& physicalSize);

	static void GlobalMemoryAcquireAndDisplay(); // visual memory display
	static void GetMemoryStats(atArray<u32>& modelinfoIds, s32 &totalMemory, s32 &drawableMemory, s32 &textureMemory, s32 &boundsMemory);
#if !__FINAL
	static void GetMemoryStatsByAllocation(atArray<u32>& modelinfoIds, s32 &totalPhysicalMemory, s32 &totalVirtualMemory, s32 &drawableMemory, s32 &drawableVirtualMemory, s32 &textureMemory, s32 &textureVirtualMemory, s32 &boundsMemory, s32 &boundsVirtualMemory);
#endif // !__FINAL
	static void DisplayObjectsInMemory(fiStream *stream, bool toScreen, s32 moduleId, const u32 fileStatusFilter, bool forceShowDummies = false);
	static void ShowBucketInfoForSelected();
	static void DisplayRequestedObjectsByModule();
	static void DisplayRequestedObjectsByModuleFiltered(bool onHdd);
	static void DisplayTotalLoadedObjectsByModule();
	static void DisplayModelStats();
	static void GetModuleMemoryUsage(s32 moduleId, s32& virtualMem, s32& physicalMem);
	static void DisplayModuleMemory(bool bPrintf, bool bOnlyShowArt, bool bShowTemporaryMemory);
	static void DisplayModuleMemoryDetailed(bool bPrintf, bool bOnlyShowArt, bool bShowTemporaryMemory);
	static void DumpModuleInfo();
	static void DumpModuleInfoAllObjects();
	static void PrintMemoryMap();
    static void SwapBuffers();
	static void ShowLowLevelStreaming();
	static void DisplayBandwidthProfiling();
    static void PrintPoolUsage();
    static void PrintStoreUsage();

	// The definition of "main" memory depends upon the platform
	static size_t GetMainMemoryUsed(const int bucket = -1);
	static size_t GetMainMemoryAvailable();
#if __DEV
	static void SetResourceForEntityFraming(strIndex streamingIndex);
	static void UpdateObjectFraming();
#endif // __DEV
#if __BANK
	static void InitWidgets();
	static void AddWidgets(bkBank& bank);
	//static s32 GetDebugStreamingObjectIndex();
	static void LogStreaming(strIndex index);
	static void PrintLoadedList();
	static void PrintRequestList();
	static void PrintLoadedListRequiredOnly();
	static void PrintLoadedListAsCSV();
	static u32 GetDependents(strIndex obj, strIndex* dependents, int maxDependents);
#endif

#if !__FINAL
	static void FullMemoryDump();
#endif // !__FINAL

	static void PrintResourceHeapPages();

	static void OutputLine(fiStream *stream, bool toScreen, Color32 color, const char *line);
	static void DisplayObjectList(fiStream *stream, bool toScreen, strIndex* pList, s32 num);

private:
	static void PrintCompactResourceHeapPages(char *buffer, int size);

	static void UpdatePageSizeStringToAllocate();
	static void AllocateResourceMemoryPage();
	static void AllocateBlocksToFragmentMemory();
	static void FreeAllAllocatedResourceMemoryPages();

	
	static void CreateDependencyString(strIndex index, bool unfulfilledOnly, char *buffer, size_t bufferSize);

	static void ComputeModuleMemoryStats(ModuleMemoryStats *memStats, bool bShowTemporaryMemory);
	static void CollectMemoryProfileInformation(int locationIndex);
	static void CollectMemoryProfileInformationFromCurrent();

	static void ShowCollectedMemoryProfileStats(bool toScreen = false);
	static void DumpCollectedMemoryProfileStats();

	static void DumpTest();
	static void CleanScene();
	static void DumpRequests();
	
	static void VMapSceneStreaming();
	static void LoadTest();

	static void WriteLargeResources();


#if __BANK
	static void DisplayUnloadedBoundsForEntity();
#endif	//__BANK
};

#if __BANK

class ResourceMemoryUseDump
{
public:
	ResourceMemoryUseDump() {}
	~ResourceMemoryUseDump() {}

	static void		AddWidgets(bkGroup &group);

	// Accessed by SmokeText.cpp now
	static void		BuildSceneCostStreamingIndices();
	static void		DumpSceneCostDetails(fiStream *pLogStream);

private:

	static void		SelectLogFileCB();

	static fiStream	*OpenLogFile();
	static void		CloseLogFile(fiStream *pLogFileHandle);
	static void		SaveToLogCB();

	// Scene Cost
	static bool		AppendToListCB(CEntity* pEntity, void* pData);
	static s32		SortByNameCB(const strIndex* pIndex1, const strIndex* pIndex2);

	static void		CalcSceneCostTotals(int &mainRam, int &vRam);


	// Required "things"?
	static void		BuildLoadedRequiredFilesCosts(int &mainRam, int &vRam);
	static void		DumpCurrentlyLoadedRequiredFiles(fiStream *pLogStream);

	// Ext Allocs
	static void		CalcExtAllocTotals(int &mainRam, int &vRam);
	static int		CompareExtAllocByAddress(const strExternalAlloc* pA, const strExternalAlloc* pB);
	static int		CompareExtAllocByStackTracehandle(const strExternalAlloc* pA, const strExternalAlloc* pB);
	static void		DumpExtAllocDetails(fiStream *pLogStream);
	static void		WriteBacktraceLine(size_t addr,const char* sym,size_t displacement);

	static fiStream *GetLogFileHandle() { return m_pLogFileHandle; }

	static char		m_LogFileName[RAGE_MAX_PATH];
	static fiStream *m_pLogFileHandle;
	static atArray<strIndex> m_SceneCostStreamingIndices;
	static bool		m_bIncludeDetailedBreakdownByCallstack;
	static bool		m_bIncludeDetailedBreakdownByContext;
};

#endif	//__BANK


#endif // STREAMING_STREAMINGDEBUG_H


