//
// streaming/streamingdebug.cpp
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//

#include "streaming/streaming_channel.h"

STREAMING_OPTIMISATIONS()

#include <algorithm>
#include <set>

#include "atl/array.h"
#include "atl/pool.h"
#include "atl/string.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
//#include "audio/audiogeometry.h"
#include "audio/northaudioengine.h"
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "bank/button.h"
#include "camera/base/basecamera.h"
#include "camera/caminterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/viewports/ViewportManager.h"
#include "camera/viewports/viewport.h"
#include "control/record.h"
#include "control/replay/replay.h"
#include "control/route.h"
#include "cutscene/cutscenemanagernew.h"
#include "data/base.h"
#include "debug/debugscene.h"
#include "debug/GtaPicker.h"
#include "debug/vectormap.h"
#include "entity/dynamicarchetype.h"
#include "entity/entity.h"
#include "diag/tracker.h"
#include "event/events.h"
#include "file/packfile.h"
#include "fragment/cache.h"
#include "fragment/cachemanager.h"
#include "fragment/manager.h"
#include "frontend/Scaleform/ScaleFormStore.h"
#include "fwanimation/animmanager.h"
#include "fwdrawlist/drawlist.h"
#include "fwscene/lod/ContainerLod.h"
#include "fwscene/mapdata/mapdata.h"
#include "fwscene/stores/blendshapestore.h"
#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/dwdstore.h"
#include "fwscene/stores/expressionsdictionarystore.h"
#include "fwscene/stores/fragmentstore.h"
#include "fwscene/stores/mapdatastore.h"
#include "fwscene/stores/maptypesstore.h"
#include "fwscene/stores/staticboundsstore.h"
#include "fwscene/stores/txdstore.h"
#include "fwscene/world/exteriorscenegraphnode.h"
#include "fwscene/world/interiorscenegraphnode.h"
#include "fwscene/world/streamedscenegraphnode.h"
#include "fwscene/world/worldlimits.h"
#include "fwsys/metadatastore.h"
#include "fwtl/pool.h"
#include "grcore/debugdraw.h"
#include "grcore/font.h"
#include "grcore/indexbuffer.h"
#include "grcore/resourcecache.h"
#include "grcore/vertexbuffer.h"
#include "grmodel/geometry.h"
#include "IslandHopper.h"
#include "modelinfo/pedmodelinfo.h"
#include "modelinfo/vehiclemodelinfo.h"
#include "modelinfo/weaponmodelinfo.h"
#include "network/network.h"
#include "network/facebook/Facebook.h"
#include "objects/dummyobject.h"
#include "objects/object.h"
#include "pathserver/pathserver.h"
#include "peds/PlayerInfo.h"
#include "peds/ped.h"
#include "peds/pedfactory.h"
#include "renderer/lights/lightentity.h"
#include "vfx/ptfx/ptfxmanager.h"
#include "scene/EntityIterator.h"
#include "scene/animatedbuilding.h"
#include "scene/building.h"
#include "scene/portals/interiorinst.h"
#include "scene/portals/portalinst.h"
#include "scene/streamer/SceneStreamerMgr.h"
#include "scene/world/gameworld.h"
#include "script/script.h"
#include "script/script_debug.h"
#include "script/streamedscripts.h"
#include "streaming/defragmentation.h"
#include "streaming/populationstreaming.h"
#include "streaming/PrioritizedClipSetStreamer.h"
#include "streaming/packfilemanager.h"
#include "streaming/streaming.h"
#include "streaming/streamingcleanup.h"
#include "streaming/streamingdebug.h"
#include "streaming/streamingdebuggraph.h"
#include "streaming/streamingengine.h"
#include "streaming/streaminginfo.h"
#include "streaming/streamingmodule.h"
#include "streaming/streamingvisualize.h"
#include "streaming/packfilemanager.h"
#include "streaming/zonedassetmanager.h"
#include "string/stringutil.h"
#include "system/controlmgr.h"
#include "system/ipc.h"
#include "system/memory.h"
#include "system/memmanager.h"
#include "system/nelem.h"
#include "system/platform.h"
#include "system/simpleallocator.h"
#include "system/typeinfo.h"
#include "system/stack.h"
#include "task/system/task.h"
#include "vector/colors.h"
#include "vehicles/vehicle.h"
#include "physics/CollisionRecords.h"

#if RSG_ORBIS
#include "grcore/wrapper_gnm.h"
#include "system/memory.h"
#endif // RSG_ORBIS

#if !__FINAL
#include "Network/Live/NetworkDebugTelemetry.h"
#endif

#if COMMERCE_CONTAINER
#include <np/sns.h>
#include <cell/sysmodule.h>
#endif

#define COMMERCE_CONTAINER_DEBUG (COMMERCE_CONTAINER && !__FINAL && 1)

#if COMMERCE_CONTAINER_DEBUG
#define COMMERCE_CONTAINER_DEBUG_ONLY(x) x
#include "system/virtualmemory_psn.h"
#else
#define COMMERCE_CONTAINER_DEBUG_ONLY(x)
#endif

#define MEMORY_LOCATION_FILE	"common:/data/debug/memoryprofilelocations." RSG_PLATFORM_ID
#define MEMORY_LOCATION_AVERAGE_NAME	"Average"

#define PROTECT_STOLEN_MEMORY 0

#if !__FINAL
PARAM(enableflush, "Enable F5 Flush for all game modes");
#endif

#if __BANK
PARAM(MemoryUsage, "Display memory usage");

namespace rage
{
	extern sysMemAllocator* g_rlAllocator;
	extern void DumpPgHandleArray(bool);
	extern bool g_AREA51;

#if PROTECT_STOLEN_MEMORY
	extern bool g_DisableReadOnly;
	extern void ProtectRange(void *ptr,size_t size,bool readOnly); // From paging\base.cpp - protects a memory range
#endif
}

extern int s_MemoryGarbageCollectionThresholdVirt;
extern int s_MemoryGarbageCollectionThresholdPhys;
extern float s_AutoCleanupCutoff;
extern float s_GarbageCollectionCutoff;
extern bool s_GarbageCollectEntitiesOnly;
extern	bool g_show_streaming_on_vmap;

// memory
static bool bDisplayMemoryUsage = false;
static bool bDisplayBucketUsage = false;
#if __DEV
static bool bDisplayMatrixUsage = false;
#endif
static bool s_printDependencies = false;
static int s_StreamerThreadPriority = PRIO_BELOW_NORMAL;
// display objects
static bool bDisplayResourceHeapPages = false;
static bool bDisplayModuleMemory = false;
static bool bDisplayModuleMemoryDetailed = false;
static bool bDisplayModuleMemoryArtOnly = false;
static bool bDisplayModuleMemoryTempMemory = false;
#if TRACK_PLACE_STATS
static bool bDisplayModulePlaceStats = false;
#endif // TRACK_PLACE_STATS
static bool bShowLoaderStatus = false;
static bool bShowAsyncStats = false;

static bool bDisplayPoolsUsage = false;
static bool bDisplayStoreUsage = false;
static bool bDisplayPackfileUsage = false;
static bool bDisplayRequestStats = false;

static bool bSortBySize = false;

// Archetype & Fragment
struct ArchetypeDebugData
{
	atString m_name;
	int m_size;
	void* m_object;
};

static bool s_EnableArchetypes = false;
static bkList* s_ArchetypeList = NULL;
atArray<ArchetypeDebugData> s_ArchetypeData;
static char s_TotalArchetypes[32] = {0};
static char s_TotalArchetypeSize[32] = {0};
static char s_ArchetypePath[128] = "x:\\archetypes.csv";

static bool s_EnableFragments = false;
static bkList* s_FragmentList = NULL;
atArray<FragmentDebugData> s_FragmentData;
static char s_TotalFragments[32] = {0};
static char s_TotalFragmentSize[32] = {0};
static char s_FragmentPath[128] = "x:\\fragments.csv";

#if __BANK
static char s_TestAssetName[RAGE_MAX_PATH];
#endif // __BANK
#if __DEV
static bool bDisplayRequestsByModule = false;
static bool bDisplayTotalLoadedObjectsByModule = false;
static bool bHideDummyRequestsOnHdd = false;
static bool bDisplayStreamingOverview = false;
// static bool bDisplayArchetypesInMemory = false;
// static bool bDisplayDrawablesInMemory = false;
// static bool bDisplayFragmentsInMemory = false;
// static bool bDisplayTxdsInMemory = false;
// static bool bDisplayDdsInMemory = false;
// static bool bDisplayAnimsInMemory = false;
// static bool bDisplayPtfxInMemory = false;
// static bool bDisplayScriptsInMemory = false;
// static bool bDisplayScaleformInMemory = false;
//	static bool bDisplayModelsUsedbyScripts = false;
// static bool bDisplayNavMeshesInMemory = false;
// static bool bDisplayCarRecordingsInMemory = false;
// static bool bDisplayHierarchicalNavNodesInMemory = false;
// static bool bDisplayAudioMeshesInMemory = false;
// static bool bDisplayPathsInMemory = false;
// static bool bDisplayArchivesInMemory = false;
// static bool bDisplayStaticBoundsInMemory = false;
// static bool bDisplayPhysicsDictsInMemory = false;
// static bool bDisplayMapDataInMemory = false;
// static bool bDisplayMapTypeDataInMemory = false;

static int s_LastRequestLines = 6;
static int s_CompletedLines = 6;
static int s_RPFCacheMissLines = 6;
static int s_OOMLines = 6;
static int s_RemovedObjectLines = 6;
extern bool g_show_streaming_on_vmap;

#endif // __DEV

static RegdEnt s_entityList[256];
static s32 s_entityListNum = 0;

static u32 sPageSizeToAllocate = 12;
static char sPageSizeToAllocateStr[8];
static bool sPageToAllocateIsVirtual;
static bool sShowStolenMemory;
static bool bDisplayStreamingInterests = false;
static bool bDrawBoundingBoxOnEntities;
static bool bSelectDontFrameEntities;
static bool bShowBucketInfoForSelected;
static atArray<void *> sAllocatedPages;
static atArray<size_t> sAllocatedPageSizes;

#if USE_DEFRAGMENTATION
static bool s_bShowDefragStatus;
#endif // USE_DEFRAGMENTATION
static u32 s_CurrentHeadPos;
static s32 gCurrentSelectedObj = -1;
static strIndex gCurrentEntityListObj;
static bool gbSeeObjDependencies = false;
static bool gbSeeObjDependents = false;
static bool s_bCleanSceneEveryFrame = false;
static bool s_bShowDepsForSelectedEntity = false;
static bool bDisplayChurnPoint = false;

static u32 s_DumpFilter = (1 << STRINFO_LOADED);
static bool s_DumpIncludeDummies = false;
static bool s_DumpExcludeSceneAssets = false;
static bool s_DumpOnlySceneAssets = false;
static bool s_DumpOnlyNoEntities = false;
static bool s_DumpOnlyZone = false;
static bool s_DumpAddDependencyCost = false;
static bool s_DumpImmediateDependencyCostOnly = false;
static bool s_DumpShowEntities = false;
static bool s_DumpIncludeInactiveEntities = false;
static bool s_DumpCachedOnly = false;
bool s_ExcludeSceneAssetsForRendering = false;
bool s_ExcludeSceneAssetsForNonRendering = false;
bool s_DumpExcludeSceneAssetsInGraph = false;
atMap<strIndex, bool> s_SceneMap;
static bool s_DumpIncludeDependencies = false;
static bool s_DumpIncludeTempMemory = false;
static bool s_DumpRequiredOnly = false;
static bool s_DumpSplitByDevice = false;
static bool bShowDependencies = false;
static bool bShowUnfulfilledDependencies = false;

bool g_SuspendEmergencyStop = false;

#if STREAM_STATS
static bool s_ShowModuleStreamingStats;
#endif // STREAM_STATS


static atArray<const char*> s_moduleNames;
char s_DumpOutputFile[RAGE_MAX_PATH];

#endif // __BANK

#include "streamingdebug_parser.h"

struct AllocInfo
{
	size_t m_VirtMem;
	size_t m_PhysMem;
	int m_AllocCount;

	AllocInfo() : m_VirtMem(0), m_PhysMem(0), m_AllocCount(0) {}
};

struct ModuleMemoryStats
{
	s32* moduleVirtualMemSize;
	s32* modulePhysicalMemSize;
	s32* moduleRequiredVirtualMemSize;
	s32* moduleRequiredPhysicalMemSize;
	s32* moduleRequestedVirtualMemSize;
	s32* moduleRequestedPhysicalMemSize;
};

#define INIT_MODULE_MEMORY_STATS(statsPtr, numModules)								\
	(statsPtr)->moduleVirtualMemSize = Alloca(s32, numModules);						\
	(statsPtr)->modulePhysicalMemSize = Alloca(s32, numModules);					\
	(statsPtr)->moduleRequiredVirtualMemSize = Alloca(s32, numModules);				\
	(statsPtr)->moduleRequiredPhysicalMemSize = Alloca(s32, numModules);			\
	(statsPtr)->moduleRequestedVirtualMemSize = Alloca(s32, numModules);			\
	(statsPtr)->moduleRequestedPhysicalMemSize = Alloca(s32, numModules);



#if __BANK
static MemoryProfileLocationList s_MemoryProfileLocations;
static int s_MemoryProfileLocationCount;
static const char **s_MemoryProfileLocationNames;
static int s_MemoryProfileLocationTarget;
static int s_NextAutomaticDebugLocation;		// Next debug location to go to while automatic cycling is enabled
static int s_CurrentCycleLocation = -1;			// Current location being visited by the automated cycle
static int s_MemoryProfileCompareLocation = -1;	// All the expected values in the modules are coming from this location
static sysTimer s_AutomaticDebugCycleTimer;
static bool s_CycleDebugLocations;
static bool s_ShowCollectedMemoryProfileStats;
typedef atArray<MemoryProfileModuleStat> MemoryProfileModuleStatList;
static float s_AutomaticCycleSeconds = 5.0f;
#endif // __BANK


#if !__WIN32PC && DEBUG_DRAW
struct pgStreamerRequestDebugInfo
{
	char m_FileName[64-12];
	u32 m_LSN;
	float m_Time;
	float m_Priority;
	u32 m_Duration;
	u32 m_Read;
	u32 m_Deflate;
	u32 m_Size;
	bool IsValid() const { return m_FileName[0] != 0; }
};

atRangeArray <pgStreamerRequestDebugInfo, 48> s_DebugInfo;
static int s_Start, s_End;
static sysCriticalSectionToken s_DebugTrackingCs;
static bool s_ShowLowLevelStreaming;

#endif // !__WIN32PC && DEBUG_DRAW

static sysTimer debugTimer;

struct MemoryStressTester
{
	enum
	{
		MAX_ALLOCS = 4,
	};

	int m_Heap;				// Heap to use, MEMTYPE_COUNT for XMemAlloc
	int m_MaxAllocCount;
	u32 m_MinAlloc;
	u32 m_MaxAlloc;
	int m_CpuId;
	int m_MinWait;
	int m_MaxWait;
	int m_Prio;
	sysIpcThreadId m_ThreadId;

	atFixedArray<void *, MAX_ALLOCS> m_Allocs;
};

static atArray<MemoryStressTester> s_MemoryStressTesters;
static MemoryStressTester s_MemoryStressTestSetup;
BANK_ONLY(static volatile bool s_DeleteAllStressTesters);

typedef enum {
	SORT_NONE,
	SORT_NAME,
	SORT_SIZE,
	SORT_VIRTUAL_SIZE,
	SORT_PHYSICAL_SIZE,
	SORT_CHUNKS_SIZE,
	SORT_NUMREFS,
	SORT_LODDISTANCE,
	SORT_FLAGS,
	SORT_IMGFILE,
	SORT_EXTRAINFO,
	SORT_LSN
} ESortDisplayedObjects;

#if __BANK
static bool s_bTrackFailedReqOOM = false;
static bool s_bTrackFailedReqFragmentation = true;
static bool s_bDisplayFailedRequests = true;
static bool s_bPauseFailedRequestList = false;
#endif

#if __BANK

static ESortDisplayedObjects s_eSortDisplayedObjects = SORT_SIZE;
static int s_moduleId = -1;
static char s_ModuleSubstringSearch[64];
static bool s_ShowModuleInfoOnScreen = false;
static bool bDisplayOnlyWithinMemRange = false;
static s32 minMemSizeToDisplayKb = 0;
static s32 maxMemSizeToDisplayKb = 512 * 1024;
//static bool bWriteDisplayedObjectsToFile = false;
// display models
#if __DEV
static bool bDisplayPedsInMemory = false;
static bool bDisplayVehiclesInMemory = false;
static bool bDisplayWeaponsInMemory = false;
static bool bDisplayTreesInMemory = false;
#endif
#endif // __BANK
fwEntity* pBreakOnCleanupEntity = NULL;

#if __PPU 
namespace rage {extern sysMemAllocator* g_pResidualAllocator;}
#endif

#if __XENON
namespace rage
{
	extern sysMemAllocator* g_pXenonPoolAllocator;
}
#endif

namespace rage
{
#if PGSTREAMER_DEBUG
XPARAM(streamingekg);
#else // PGSTREAMER_DEBUG
XPARAM(streamingekg);
#endif // PGSTREAMER_DEBUG
}
XPARAM(emergencystop);

#if __BANK
void UpdateThreadPriority()
{
	pgStreamer::SetThreadPriority((sysIpcPriority) s_StreamerThreadPriority);
}
#endif // __BANK

const char *CreateFlagString(u32 flags, char *outBuffer, size_t bufferSize)
{
	outBuffer[0] = 0;

	if(flags & STRFLAG_DONTDELETE)
		safecat(outBuffer, " DONT", bufferSize);
	if(flags & STRFLAG_MISSION_REQUIRED)
		safecat(outBuffer, " MISS", bufferSize);
	if(flags & STRFLAG_CUTSCENE_REQUIRED)
		safecat(outBuffer, " CUT", bufferSize);
	if(flags & STRFLAG_INTERIOR_REQUIRED)
		safecat(outBuffer, " INT", bufferSize);
	if(flags & STRFLAG_LOADSCENE)
		safecat(outBuffer, " LOADSCENE", bufferSize);
	if(flags & STRFLAG_PRIORITY_LOAD)
		safecat(outBuffer, " PRIO", bufferSize);
	if(flags & STRFLAG_FORCE_LOAD)
		safecat(outBuffer, " FORCE", bufferSize);
	if(flags & STRFLAG_ZONEDASSET_REQUIRED)
		safecat(outBuffer, " ZONED", bufferSize);

	return outBuffer;
}

static void AddToAssetListRecursive(strIndex index, atMap<strIndex, bool> &outAssetList)
{
	if (outAssetList.Access(index) == NULL)
	{
		outAssetList.Insert(index, true);

		strIndex deps[256];
		strStreamingModule *module = strStreamingEngine::GetInfo().GetModule(index);

		int depCount = module->GetDependencies(module->GetObjectIndex(index), deps, NELEM(deps));

		for (int x=0; x<depCount; x++)
		{
			AddToAssetListRecursive(deps[x], outAssetList);
		}
	}
}

void AddEntityAssets(const CStreamingBucketSet &bucketSet, float minScore, atMap<strIndex, bool> &outAssetList)
{
	int count = bucketSet.GetEntryCount();

	for (int x=0; x<count; x++)
	{
		const BucketEntry &entry = bucketSet.GetEntries()[x];
		if (entry.m_Score >= minScore && entry.IsStillValid())
		{
			// Get all the assets for this entity.
			const CEntity *entity = entry.GetEntity();
			strIndex index = fwArchetypeManager::GetStreamingModule()->GetStreamingIndex(strLocalIndex(entity->GetModelIndex()));

			AddToAssetListRecursive(index, outAssetList);
		}
	}
}

void GetActiveEntityAssets(atMap<strIndex, bool> &outAssetList)
{
	g_SceneStreamerMgr.GetStreamingLists().WaitForSort();

	USE_DEBUG_MEMORY();
	AddEntityAssets(g_SceneStreamerMgr.GetStreamingLists().GetActiveEntityList().GetBucketSet(), (float)STREAMBUCKET_INVISIBLE_FAR, outAssetList);
	AddEntityAssets(g_SceneStreamerMgr.GetStreamingLists().GetNeededEntityList().GetBucketSet(), (float)STREAMBUCKET_INVISIBLE_FAR, outAssetList);
}

#if __BANK
void CStreamingDebug::StorePvsIfNeeded()
{
	if (s_DumpExcludeSceneAssetsInGraph || s_DumpExcludeSceneAssets || s_DumpOnlySceneAssets)
	{
		USE_DEBUG_MEMORY();

		s_SceneMap.Reset();

		CPostScan::WaitForSortPVSDependency();
		CGtaPvsEntry* pPvsStart = gPostScan.GetPVSBase();
		CGtaPvsEntry* pPvsStop = gPostScan.GetPVSEnd();
		strStreamingModule *archetypeModule = fwArchetypeManager::GetStreamingModule();

		bool forRenderingOnly = s_ExcludeSceneAssetsForRendering;
		bool nonRenderingOnly = s_ExcludeSceneAssetsForNonRendering;

		bool checkForRendering = (forRenderingOnly || nonRenderingOnly);

		while (pPvsStart != pPvsStop)
		{
			bool neededForRendering = (!(pPvsStart->GetVisFlags().GetOptionFlags() & VIS_FLAG_ONLY_STREAM_ENTITY));
			if ((!checkForRendering) || (neededForRendering == forRenderingOnly))
			{
				const CEntity* pEntity = pPvsStart->GetEntity();

				if (pEntity)
				{
					u32 modelIndex = pEntity->GetModelIndex();
					AddToAssetListRecursive(archetypeModule->GetStreamingIndex(strLocalIndex(modelIndex)), s_SceneMap);
				}
			}

			++pPvsStart;
		}
	}
}
#endif // __BANK

void GetSizesSafe(strIndex index, strStreamingInfo &info, int &outVirtSize, int &outPhysSize, bool includeTempMemory)
{
	outVirtSize = info.ComputeOccupiedVirtualSize(index, true, includeTempMemory);
	outPhysSize = info.ComputePhysicalSize(index, true);
}

void AddDependencyCost(strIndex index, int &virtSize, int &physSize, atMap<strIndex, bool> &dependencies, bool recursive)
{
	strStreamingModule *module = strStreamingEngine::GetInfo().GetModule(index);
	strLocalIndex objIndex = module->GetObjectIndex(index);
	strIndex deps[256];

	int depCount = module->GetDependencies(objIndex, deps, NELEM(deps));

	for (int x=0; x<depCount; x++)
	{
		if (dependencies.Access(deps[x]) == NULL)
		{
			dependencies.Insert(deps[x], true);
			int depVirtSize, depPhysSize;

			GetSizesSafe(deps[x], strStreamingEngine::GetInfo().GetStreamingInfoRef(index), depVirtSize, depPhysSize, false);
			virtSize += depVirtSize;
			physSize += depPhysSize;

			if (recursive)
			{
				AddDependencyCost(deps[x], virtSize, physSize, dependencies, recursive);
			}
		}
	}
}

void ComputeDependencyCost(strIndex index, int &virtSize, int &physSize, bool recursive)
{
	strStreamingModule *module = strStreamingEngine::GetInfo().GetModule(index);
	strLocalIndex objIndex = module->GetObjectIndex(index);
	strIndex deps[256];

	int depCount = module->GetDependencies(objIndex, deps, NELEM(deps));

	for (int x=0; x<depCount; x++)
	{
		int depVirtSize, depPhysSize;

		GetSizesSafe(deps[x], strStreamingEngine::GetInfo().GetStreamingInfoRef(index), depVirtSize, depPhysSize, false);
		virtSize += depVirtSize;
		physSize += depPhysSize;

		if (recursive)
		{
			ComputeDependencyCost(deps[x], virtSize, physSize, recursive);
		}
	}
}


void GetLoadedSizesSafe(strStreamingModule &module, strLocalIndex objIndex, int &outVirtSize, int &outPhysSize)
{
	strStreamingInfo &info = *module.GetStreamingInfo(objIndex);

	if (info.GetStatus() == STRINFO_LOADED)
	{
		outVirtSize = (int) module.GetVirtualMemoryOfLoadedObj(objIndex, false);
		outPhysSize = (int) module.GetPhysicalMemoryOfLoadedObj(objIndex, false);
	}
	else
	{
		GetSizesSafe(module.GetStreamingIndex(objIndex), info, outVirtSize, outPhysSize, false);
	}
}

void SwitchToMainCCS()
{
	const CDataFileMgr::ContentChangeSet *ccs = DATAFILEMGR.GetContentChangeSet(atHashString("MOUNT_PROLOGUE"));
	Assert(ccs);
	CFileLoader::RevertContentChangeSet(*ccs, CDataFileMgr::ChangeSetAction::CCS_ALL_ACTIONS);
	DATAFILEMGR.Load("common:/data/levels/gta5/gta5_main.meta");
	ccs = DATAFILEMGR.GetContentChangeSet(atHashString("MOUNT_GAME"));
	Assert(ccs);
	CFileLoader::ExecuteContentChangeSet(*ccs);

}

// For archetypes: True if there is at least one entity with a draw handler using thie archetype.
// Always returns false for non-archetypes.
bool HasEntities(strIndex index)
{
	strStreamingModule *module = strStreamingEngine::GetInfo().GetModule(index);

	if (module != CModelInfo::GetStreamingModule())
	{
		return false;
	}

	s32 objIndex = module->GetObjectIndex(index).Get();

	const fwLinkList<ActiveEntity>& activeList = g_SceneStreamerMgr.GetStreamingLists().GetActiveList();

	fwLink<ActiveEntity>* pLastLink = activeList.GetLast()->GetPrevious();

	while(pLastLink != activeList.GetFirst())
	{
		CEntity* pEntity = static_cast<CEntity*>(pLastLink->item.GetEntity());
		pLastLink = pLastLink->GetPrevious();

		if (pEntity)
		{
			const int modelIndex = pEntity->GetModelIndex();

			if (modelIndex == objIndex)
			{
				return true;
			}
		}
	}

	return false;
}

#if __BANK
void ForceLoadTestAsset()
{
	strIndex index = strStreamingEngine::GetInfo().GetStreamingIndexByName(s_TestAssetName);

	if (streamVerifyf(index.IsValid(), "Asset %s not found", s_TestAssetName))
	{
		strStreamingEngine::GetInfo().RequestObject(index, STRFLAG_FORCE_LOAD);
	}
}

void UnloadTestAsset()
{
	strIndex index = strStreamingEngine::GetInfo().GetStreamingIndexByName(s_TestAssetName);

	if (streamVerifyf(index.IsValid(), "Asset %s not found", s_TestAssetName))
	{
		strStreamingEngine::GetInfo().RemoveObject(index);
	}
}
#endif // __BANK

bool MemoryProfileLocation::IsMetaLocation() const
{
	return strcmp(m_Name.c_str(), MEMORY_LOCATION_AVERAGE_NAME) == 0;
}

void MemoryProfileLocationList::ComputeAverages()
{
	int locCount = m_Locations.GetCount();

	// The first element needs to be the average element, or else we have to create it.
	if (locCount == 0 || !m_Locations[0].IsMetaLocation())
	{
		// Add an "average" element. Since the size has been predetermined, we need to create a new array.
		atArray<MemoryProfileLocation> tempArray;
		tempArray.Swap(m_Locations);

		m_Locations.Resize(locCount+1);
		m_Locations[0].m_Name = MEMORY_LOCATION_AVERAGE_NAME;

		for (int x=0; x<locCount; x++)
		{
			m_Locations[x+1] = tempArray[x];
		}

		locCount++;
	}

	if (locCount > 1)
	{
		MemoryProfileLocation &avgLog = m_Locations[0];

		// Let's assume everything uses the same number of modules, so we'll randomly pick
		// the first element as our authoritative list of modules.
		int moduleCount = m_Locations[1].m_MemoryFootprint.m_ModuleMemoryStatsMap.GetCount();
		avgLog.m_MemoryFootprint.m_ModuleMemoryStatsMap.Reset();
		avgLog.m_MemoryFootprint.m_ModuleMemoryStatsMap.Reserve(moduleCount);

		for (int module=0; module<moduleCount; module++)
		{
			u64 count = 0;
			u64 virtSum = 0, physSum = 0;
			atHashString *key = m_Locations[1].m_MemoryFootprint.m_ModuleMemoryStatsMap.GetKey(module);

			for (int x=1; x<locCount; x++)
			{
				MemoryProfileLocation &loc = m_Locations[x];
				MemoryProfileModuleStat *stat = loc.m_MemoryFootprint.m_ModuleMemoryStatsMap.SafeGet(*key);

				if (stat)
				{
					count++;
					virtSum += (u64) stat->m_VirtualMemory;
					physSum += (u64) stat->m_PhysicalMemory;
				}
			}

			virtSum /= count;
			physSum /= count;

			MemoryProfileModuleStat *newStat = avgLog.m_MemoryFootprint.m_ModuleMemoryStatsMap.Insert(*key);
			newStat->m_VirtualMemory = (s32) virtSum;
			newStat->m_PhysicalMemory = (s32) physSum;
		}

		avgLog.m_MemoryFootprint.m_ModuleMemoryStatsMap.FinishInsertion();
	}
}



void CStreamingDebug::CleanScene()
{
	strStreamingEngine::GetInfo().ResetInfoIteratorForDeletingNextLeastUsed();
	while (strStreamingEngine::GetInfo().DeleteNextLeastUsedObject(0)) {}
}

#if STREAM_STATS
void ShowModuleStreamingStats()
{
	strStreamingModuleMgr &mgr = strStreamingEngine::GetInfo().GetModuleMgr();
	int modules = mgr.GetNumberOfModules();

	grcDebugDraw::AddDebugOutputEx(false, "%20s %10s %6s %10s %5s %s",
		"Module", "Data Read", "Loaded", "Data Cncld", "Cncld", "% of all loaded");

	int totalLoadCount = 0;

	for (int x=0; x<modules; x++)
	{
		totalLoadCount += mgr.GetModule(x)->GetFilesLoaded();
	}

	// Prevent division by 0 - if this is 0, we know that all values are 0, so we're just dividing 0 by 1.
	if (totalLoadCount == 0)
	{
		totalLoadCount = 1;
	}

	float totalLoadCountf = (float) totalLoadCount / 100.0f;
	
	for (int x=0; x<modules; x++)
	{
		strStreamingModule *module = mgr.GetModule(x);

		float pctLoaded = (float) module->GetFilesLoaded() / totalLoadCountf;

		grcDebugDraw::AddDebugOutputEx(false, "%20s %9.1fM %6d %9.1fM %5d %.2f%%",
			module->GetModuleName(), (float) module->GetDataStreamed() / (1024.0f * 1024.0f), module->GetFilesLoaded(),
			(float) module->GetDataCanceled() / (1024.0f * 1024.0f), module->GetFilesCanceled(), pctLoaded);
	}
}

void ResetModuleStreamingStats()
{
	strStreamingModuleMgr &mgr = strStreamingEngine::GetInfo().GetModuleMgr();
	int modules = mgr.GetNumberOfModules();

	for (int x=0; x<modules; x++)
	{
		mgr.GetModule(x)->ResetStreamingStats();
	}
}
#endif // STREAM_STATS


#if __DEV
// function prototypes
template<class _T> void DisplayModelsInMemory(fwArchetypeDynamicFactory<_T>& store);
template<class _T> void WriteModelSizes(fwArchetypeDynamicFactory<_T>& store);

#if __DEV 
void WriteVehicleModelSizes() { WriteModelSizes<CVehicleModelInfo>(CModelInfo::GetVehicleModelInfoStore());}
void WritePedModelSizes() { WriteModelSizes<CPedModelInfo>(CModelInfo::GetPedModelInfoStore());}
#endif

#endif //#if __DEV 

void SanityCheckMemoryHeaps();

#if __BANK
void WriteMemoryReport();

#if USE_DEFRAGMENTATION
void AbortDefrag() {strStreamingEngine::GetDefragmentation()->Abort();}
#endif // USE_DEFRAGMENTATION

void DisplayPoolsUsage();
void DisplayStoreUsage();
void DisplayPackfileUsage();
#if __DEV
static u32 s_largeResourceSize = 1;
void DisplayMatrixUsage();
#endif // __DEV
void DisplayMemoryUsage();
void DisplayMemoryBucketUsage();
void ResetLoadedCounts();


void CheckForFlagClear(u32 ASSERT_ONLY(flag)){

#if __ASSERT
	strLoadedInfoIterator LoadedStreamInfos;
	strIndex index = LoadedStreamInfos.GetNextIndex();
	while(index.IsValid()){
		strStreamingInfo* pCurrStreamingInfo = strStreamingEngine::GetInfo().GetStreamingInfo(index);
		streamAssertf((pCurrStreamingInfo->GetFlags() & flag)==0, "Flag 0x%x for %s isn\'t clear", flag, strStreamingEngine::GetObjectName(index));
		index = LoadedStreamInfos.GetNextIndex();
	}
#endif // __ASSERT
}

void CheckForHeldObjInteriorCB(void){
	CheckForFlagClear(STRFLAG_INTERIOR_REQUIRED);
}

void CheckForHeldObjCutsceneCB(void){
	CheckForFlagClear(STRFLAG_CUTSCENE_REQUIRED);
}

void ResetStreamingStats()
{
	strStreamingEngine::GetInfo().ResetStreamingStats();
}

//////////////////////////////////////////////////////////////////////////
// Streaming log
#include "game\clock.h"

bool		s_bLogStreaming = false;
bool		s_bLogAppend = false;
char		s_LogStreamingName[256] = "X:\\gta\\build\\streamlog.txt";
fiStream*	s_LogStream = NULL;

void StartLog()
{
	streamAssertf(!s_LogStream, "Log stream is already open");

	if (s_bLogAppend) {
		s_LogStream = s_LogStreamingName[0]?fiStream::Open(s_LogStreamingName, fiDeviceLocal::GetInstance(), false):NULL;
		s_LogStream->Seek(s_LogStream->Size()); 
	}

	if (!s_LogStream)
		s_LogStream = s_LogStreamingName[0]?fiStream::Create(s_LogStreamingName, fiDeviceLocal::GetInstance()):NULL;

	if (!s_LogStream) {
		s_bLogStreaming =false;
		streamErrorf("Could not create '%s'", s_LogStreamingName);
	}
	else {
		s_bLogStreaming = true;
		fprintf(s_LogStream, "%d start log session\r\n", fwTimer::GetSystemFrameCount());
		fflush(s_LogStream);
	}
}

void EndLog()
{
	streamAssertf(s_LogStream, "Log stream doesn't exist");

	fprintf(s_LogStream, "%d end log session\r\n", fwTimer::GetSystemFrameCount());
	fflush(s_LogStream);
	s_LogStream->Close();
	s_LogStream = NULL;
	s_bLogStreaming = false;
}

void ChangeStreamingLog()
{
	if (s_LogStream)
		EndLog();
}

void SelectStreamingLog()
{
	if (!BANKMGR.OpenFile(s_LogStreamingName, 256, "*.txt", true, "Streaming log (*.txt)"))
		s_LogStreamingName[0] = '\0';

	ChangeStreamingLog();
}

void ToggleStreamingLog()
{
	if (!s_LogStream)
		StartLog();
	else
		EndLog();
}

void CStreamingDebug::LogStreaming(strIndex index)
{
	if (!s_LogStream)
		return;

	strStreamingInfo* pInfo = strStreamingEngine::GetInfo().GetStreamingInfo(index);

	// image
	const char* packfileName = strPackfileManager::GetImageFileNameFromHandle(pInfo->GetHandle());

	// player pos
	Vector3 vPlayerPos = CPlayerInfo::ms_cachedMainPlayerPos;

	// camera
	const Vector3& camPos = camInterface::GetPos();
	Vector3 vCamDir;
	camInterface::GetMat().ToEulersXYZ(vCamDir);
	vCamDir.x = ( RtoD * vCamDir.x);  // convert as level guys work on Degrees, not Radians
	vCamDir.y = ( RtoD * vCamDir.y);
	vCamDir.z = ( RtoD * vCamDir.z);

	int virtSize, physSize;
	GetSizesSafe(index, *pInfo, virtSize, physSize, false);

	fprintf(s_LogStream, "%d, %s, %s, %d, %d, (%d:%d), %0.2f, %0.2f, %0.2f, %0.2f, %0.2f, %0.2f, %0.2f, %0.2f, %0.2f\r\n", 
		fwTimer::GetSystemFrameCount(), 
		strStreamingEngine::GetObjectName(index), 
		packfileName, 
		virtSize, physSize, 
		CClock::GetHour(), CClock::GetMinute(), 
		vPlayerPos.x, vPlayerPos.y, vPlayerPos.z, 
		camPos.x, camPos.y, camPos.z, 
		vCamDir.x, vCamDir.y, vCamDir.z);

	fflush(s_LogStream);
}

// StreamingLog
//////////////////////////////////////////////////////////////////////////

void DumpModuleMemory()
{
	CStreamingDebug::DisplayModuleMemoryDetailed(true, false, true);
}

u32 CStreamingDebug::GetDependents(strIndex obj, strIndex* dependents, int maxDependents)
{
	u32 count = 0;

	strLoadedInfoIterator loadedStreamInfos;
	strIndex index = loadedStreamInfos.GetNextIndex();
	while (index.IsValid()){

		strIndex deps[STREAMING_MAX_DEPENDENCIES];
		strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModule(index);
		s32 numDeps = pModule->GetDependencies(pModule->GetObjectIndex(index), &deps[0], STREAMING_MAX_DEPENDENCIES);
		for (s32 j=0; j<numDeps; j++)
		{
			if(deps[j] == obj)
			{
				dependents[count++] = index;

				if (count >= maxDependents)
					return count;
			}
		}

		index = loadedStreamInfos.GetNextIndex();
	}

	return count;
}

const char **CStreamingDebug::GetMemoryProfileLocationNames()
{
	return s_MemoryProfileLocationNames;
}

const MemoryProfileLocationList &CStreamingDebug::GetMemoryProfileLocationList()
{
	return s_MemoryProfileLocations;
}

int CStreamingDebug::GetMemoryProfileLocationCount()
{
	return s_MemoryProfileLocationCount;
}

void CStreamingDebug::LoadMemoryProfileLocations()
{
	if (ASSET.Exists(MEMORY_LOCATION_FILE, "xml"))
	{
		PARSER.LoadObject(MEMORY_LOCATION_FILE, "xml", s_MemoryProfileLocations);
		s_MemoryProfileLocations.ComputeAverages();

		// Create a profile location name array so the combo box can use it
		delete[] s_MemoryProfileLocationNames;
		int count = s_MemoryProfileLocations.m_Locations.GetCount();
		s_MemoryProfileLocationCount = count;
		s_MemoryProfileLocationNames = rage_new const char *[count];

		for (int x=0; x<count; x++)
		{
			s_MemoryProfileLocationNames[x] = s_MemoryProfileLocations.m_Locations[x].m_Name.c_str();
		}
	}
	else
	{
		Errorf("Couldn't load %s.xml", MEMORY_LOCATION_FILE);
	}
}


#endif // __BANK

#if !__FINAL
#if __BANK
static void StreamDisplayBacktraceLine(size_t addr,const char* sym,size_t displacement)
{
	streamDisplayf("0x%" SIZETFMT "x - %s - %" SIZETFMT "u", addr, sym, displacement);
}
#endif // __BANK

void CStreamingDebug::FullMemoryDump()
{
	streamDisplayf("==== Full Memory Dump =====");
	DumpModuleMemoryComparedToAverage();

#if __BANK
#if ENABLE_DEFRAGMENTATION
	streamDisplayf("==== pgHandles =====");
	rage::DumpPgHandleArray(false);
#endif

	streamDisplayf("LOADED OBJECTS:");

	// Preserve and disable some of the filters.
	char substringSearch = s_ModuleSubstringSearch[0];
	bool splitByDevice = s_DumpSplitByDevice;

	s_ModuleSubstringSearch[0] = 0;
	s_DumpSplitByDevice = false;	

	CStreamingDebug::DisplayObjectsInMemory(NULL, false, -1, (1 << STRINFO_LOADED) | (1 << STRINFO_LOADING), true);

	// Restore the filters.
	s_ModuleSubstringSearch[0] = substringSearch;
	s_DumpSplitByDevice = splitByDevice;
#endif // __BANK

	streamDisplayf("====== Draw List Page Dump ======");

	gDCBuffer->DumpDrawListPages();

#if __BANK
	streamDisplayf("====== Module Summary ======");
	DumpModuleMemory();
#endif // __BANK

	streamDisplayf("====== Buddy Heap Pages ======");
	PrintResourceHeapPages();

	streamDisplayf("====== Heap Overview ======");
	PrintMemoryMap();

	streamDisplayf("====== Entities ======");
	streamDisplayf("Buildings: %d/%d", CBuilding::GetPool()->GetNoOfUsedSpaces(), CBuilding::GetPool()->GetSize());
	streamDisplayf("Animated Buildings: %d/%d", CAnimatedBuilding::GetPool()->GetNoOfUsedSpaces(), CAnimatedBuilding::GetPool()->GetSize());
	streamDisplayf("Dummy Objects: %d/%d", CDummyObject::GetPool()->GetNoOfUsedSpaces(), CDummyObject::GetPool()->GetSize());
	streamDisplayf("Peds: %d/%d", CPed::GetPool()->GetNoOfUsedSpaces(), CPed::GetPool()->GetSize());

#if __BANK
	streamDisplayf("====== External Allocations ======");
	atArray<strExternalAlloc> &allocs = strStreamingEngine::GetAllocator().m_ExtAllocTracker.m_Allocations;

	{
		USE_DEBUG_MEMORY();

		atMap<u32, AllocInfo> bucketToMemory;
		atMap<u32, AllocInfo> callstackToMemory;
		int allocCount = allocs.GetCount();

		for (int x=0; x<allocCount; x++)
		{
			strExternalAlloc &alloc = allocs[x];
			AllocInfo *allocInfo;

			for (int pass=0; pass<2; pass++)
			{
				if (pass == 0)
				{
					allocInfo = &bucketToMemory[alloc.m_BucketID];
				}
				else
				{
					allocInfo = &callstackToMemory[alloc.m_BackTraceHandle];
				}

				if (alloc.IsMAIN())
				{
					allocInfo->m_VirtMem += alloc.m_Size;
				}
				else
				{
					allocInfo->m_PhysMem += alloc.m_Size;
				}

				allocInfo->m_AllocCount++;
			}
		}

		atMap<u32, AllocInfo>::Iterator cit = callstackToMemory.CreateIterator();
		for (cit.Start(); !cit.AtEnd(); cit.Next())
		{
			AllocInfo &allocInfo = cit.GetData();
			streamDisplayf("%dKB/%dKB (%d allocs):", (u32) (allocInfo.m_VirtMem / 1024), (u32) (allocInfo.m_PhysMem / 1024), allocInfo.m_AllocCount);
			sysStack::PrintRegisteredBacktrace((u16) cit.GetKey(), StreamDisplayBacktraceLine);
		}

		streamDisplayf("==== External Bucket Summary =======");

		atMap<u32, AllocInfo>::Iterator it = bucketToMemory.CreateIterator();
		for (it.Start(); !it.AtEnd(); it.Next())
		{
			const char * barName = strStreamingEngine::GetAllocator().GetBucketName(it.GetKey());
			AllocInfo &allocInfo = it.GetData();
			streamDisplayf("%s: %dKB/%dKB (%d allocs):", barName, (u32) (allocInfo.m_VirtMem / 1024), (u32) (allocInfo.m_PhysMem / 1024), allocInfo.m_AllocCount);
		}
	}
#endif // __BANK

	streamDisplayf("====== Streaming Interests ======");
	g_SceneStreamerMgr.DumpStreamingInterests();

	streamDisplayf("Used by external alloc: %" SIZETFMT "dK virt, %" SIZETFMT "dK phys",
		strStreamingEngine::GetAllocator().GetExternalVirtualMemoryUsed() / 1024, strStreamingEngine::GetAllocator().GetExternalPhysicalMemoryUsed() / 1024 );

	streamDisplayf("Master Cutoff: %f", g_SceneStreamerMgr.GetStreamingLists().GetMasterCutoff());

	streamDisplayf("==== End Full Memory Dump =====");
}
#endif // !__FINAL

void LoadAllModuleObjectsOnce(s32 moduleId)
{
	strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(moduleId);

	for(s32 obj = 0; obj<pModule->GetCount(); obj++)
	{
		if(!pModule->HasObjectLoaded(strLocalIndex(obj)) &&
			pModule->IsObjectInImage(strLocalIndex(obj)) &&
			!pModule->GetStreamingInfo(strLocalIndex(obj))->IsFlagSet(STRFLAG_INTERNAL_DUMMY))
		{
			streamDisplayf("checking %s", strStreamingEngine::GetInfo().GetObjectName(pModule->GetStreamingIndex(strLocalIndex(obj))));
			pModule->StreamingRequest(strLocalIndex(obj), STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
			CStreaming::LoadAllRequestedObjects();
			pModule->StreamingRemove(strLocalIndex(obj));
		}
	}
}

void LoadAllObjectsOnce()
{
	LoadAllModuleObjectsOnce(CModelInfo::GetStreamingModuleId());
	LoadAllModuleObjectsOnce(g_DwdStore.GetStreamingModuleId());
}

strStreamingFile* GetImageFile(const char* filename)
{
	s32 count = strPackfileManager::GetImageFileCount(); 

	u32 hashvalue = atStringHash(filename);

	for (s32 index = 0; index < count; ++index)
	{
		strStreamingFile* file = strPackfileManager::GetImageFile(index);
		if (file->m_name.GetHash() == hashvalue)
			return file;
	}
	return NULL;
}

void LoadImage(const char* filename)
{
	strStreamingFile* file = GetImageFile(filename);
	if (file)
	{
		u32 archiveMask = (file->m_packfile->GetPackfileIndex() << 16 /*ArchiveShift*/);
		strStreamingInfoIterator it;
		while (!it.IsAtEnd())
		{
			strStreamingInfo* info = strStreamingEngine::GetInfo().GetStreamingInfo(*it);

			if (info && ((info->GetHandle() & 0xFFFF0000) == archiveMask))
				strStreamingEngine::GetInfo().RequestObject(*it, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);

			++it;
		}
	}
}

void ImageSize(const char* filename)
{
	strStreamingFile* file = GetImageFile(filename);
	size_t virtualSize = 0;
	size_t physicalSize = 0;

	if (file)
	{
		u32 archiveMask = (file->m_packfile->GetPackfileIndex() << 16 /*ArchiveShift*/);

		strStreamingInfoIterator it;
		while (!it.IsAtEnd())
		{
			strStreamingInfo* info = strStreamingEngine::GetInfo().GetStreamingInfo(*it);
			if (info && ((info->GetHandle() & 0xFFFF0000) == archiveMask))
			{
				int virtSize, physSize;
				GetSizesSafe(*it, *info, virtSize, physSize, false);

				virtualSize += virtSize;
				physicalSize += physSize;
			}

			++it;
		}
	}
	streamDisplayf("Image %s %" SIZETFMT "u(%" SIZETFMT "u) %" SIZETFMT "uk (%" SIZETFMT "u)", filename, virtualSize>>10, virtualSize, physicalSize>>10, physicalSize);
}

void ResetPlaceStats()
{
#if TRACK_PLACE_STATS
	strStreamingEngine::GetInfo().GetModuleMgr().ResetPlaceStats();
#endif // TRACK_PLACE_STATS
}

#if __BANK
void CStreamingDebug::PrintLoadedList()
{
	strStreamingEngine::GetInfo().PrintLoadedList(true, s_printDependencies, false);
}

void CStreamingDebug::PrintLoadedListRequiredOnly()
{
	strStreamingEngine::GetInfo().PrintLoadedList(true, s_printDependencies, false, true);
}

void CStreamingDebug::PrintRequestList()
{
	strStreamingEngine::GetInfo().PrintLoadedList(true, s_printDependencies, false, false, STRINFO_LOADREQUESTED);
}

void DumpModuleInfoHelper(const bool allObjects)
{
#if __BANK
	fiStream *logStream = NULL;

	// Open file if that's what we chose.
	if (s_DumpOutputFile[0])
	{
		logStream = ASSET.Create(s_DumpOutputFile, "");

		if (!logStream)
		{
			streamErrorf("Could not create output file %s, writing to TTY instead.", s_DumpOutputFile);
		}
	}

	CStreamingDebug::DisplayObjectsInMemory(logStream, false, s_moduleId - 1, allObjects ? ~0 : s_DumpFilter);

	if (logStream)
	{
		logStream->Close();
	}
#else
	(void) allObjects;
#endif // __BANK
}

void CStreamingDebug::DumpModuleInfo() {
	DumpModuleInfoHelper( false );
}

void CStreamingDebug::DumpModuleInfoAllObjects() {
	DumpModuleInfoHelper( true );
}

void CStreamingDebug::PrintLoadedListAsCSV()
{
	strStreamingEngine::GetInfo().PrintLoadedList(true, s_printDependencies, true);
}
#endif // __BANK

//
// name:		CalculateRequestedObjectSizes
// description:	Calculate the memory sizes
void CStreamingDebug::CalculateRequestedObjectSizes(s32& virtualSize, s32& physicalSize)
{
	if (strStreamingEngine::IsInitialised())
	{
		RequestInfoList::Iterator it(strStreamingEngine::GetInfo().GetRequestInfoList()->GetFirst());
		while (it.IsValid())
		{		
			strStreamingInfo& info = strStreamingEngine::GetInfo().GetStreamingInfoRef(it.Item()->m_Index);

			int virtSize, physSize;
			GetSizesSafe(it.Item()->m_Index, info, virtSize, physSize, false);

			virtualSize += virtSize;
			physicalSize += physSize;
			it.Next();
		}
	}
}


//
// name:		CStreamingDebug::GetModuleMemoryUsage
// description:	Return memory usage of one streaming module
void CStreamingDebug::GetModuleMemoryUsage(s32 moduleId, s32& virtualMem, s32& physicalMem)
{
	virtualMem = 0;
	physicalMem = 0;

	strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(moduleId);
	// add up memory
	for(s32 obj = 0; obj<pModule->GetCount(); obj++)
	{
		if(pModule->HasObjectLoaded(strLocalIndex(obj)) &&
			pModule->IsObjectInImage(strLocalIndex(obj)))
		{
			int virtSize, physSize;
			GetLoadedSizesSafe(*pModule, strLocalIndex(obj), virtSize, physSize);

			virtualMem += virtSize;
			physicalMem += physSize;
		}
	}
}

//
// name:		CStreamingDebug::DisplayModuleMemory
// description:	Display memory each module uses
void CStreamingDebug::DisplayModuleMemory(bool bPrintf, bool bOnlyShowArt, bool bShowTemporaryMemory)
{
	strStreamingModuleMgr& moduleMgr = strStreamingEngine::GetInfo().GetModuleMgr();
	s32 numModules = moduleMgr.GetNumberOfModules();
	s32* moduleVirtualMemSize = Alloca(s32, numModules);
	s32* modulePhysicalMemSize = Alloca(s32, numModules);
	s32 moduleId;


	// clear arrays
	for(moduleId=0; moduleId<numModules; moduleId++)
	{
		moduleVirtualMemSize[moduleId] = 0;
		modulePhysicalMemSize[moduleId] = 0;
	}

	// count up memory for each module while ignoring extra loaders
	for(moduleId=0; moduleId<numModules; moduleId++)
	{
		strStreamingModule* pModule = moduleMgr.GetModule(moduleId);
		// add up memory
		for(s32 obj = 0; obj<pModule->GetCount(); obj++)
		{
			bool measured = false;
			if (pModule->IsObjectInImage(strLocalIndex(obj)))
			{
				if (bShowTemporaryMemory || !pModule->RequiresTempMemory(strLocalIndex(obj)))
				{
					measured = true;
					int virtualSize, physicalSize;
					GetLoadedSizesSafe(*pModule, strLocalIndex(obj), virtualSize, physicalSize);

					if (pModule->HasObjectLoaded(strLocalIndex(obj)))
					{
						moduleVirtualMemSize[moduleId] += virtualSize;
						modulePhysicalMemSize[moduleId] += physicalSize;
					}
				} 

				// if the module wasn't already counted (because it uses temp memory) just add the extra memory
				if (!measured && pModule->UsesExtraMemory())
				{
					int virtualSize, physicalSize;
					virtualSize = (int)pModule->GetExtraVirtualMemory(strLocalIndex(obj));
					physicalSize = (int)pModule->GetExtraPhysicalMemory(strLocalIndex(obj));

					if (pModule->HasObjectLoaded(strLocalIndex(obj)))
					{
						moduleVirtualMemSize[moduleId] += virtualSize;
						modulePhysicalMemSize[moduleId] += physicalSize;
					}
				}	
			}
		}
	}

#if DEBUG_DRAW
	if (!bPrintf)
{	
		grcDebugDraw::AddDebugOutputEx(false,
			" Name            Loaded (Main/Vram)");
	}
#endif // DEBUG_DRAW

	s32 totalVirtualMemSize = 0;
	s32 totalPhysicalMemSize = 0;

	bool color = true;

	// Print out results
	for(moduleId = 0; moduleId < numModules; moduleId++)
	{
		strStreamingModule* pModule = moduleMgr.GetModule(moduleId);

		if (bOnlyShowArt)
		{
			if (pModule->GetStreamingModuleId() != g_TxdStore.GetStreamingModuleId()		&&
				pModule->GetStreamingModuleId() != g_DwdStore.GetStreamingModuleId()		&&
				pModule->GetStreamingModuleId() != g_DrawableStore.GetStreamingModuleId()	&&
				pModule->GetStreamingModuleId() != g_FragmentStore.GetStreamingModuleId())
			{
				continue; 
			}
		}

		totalVirtualMemSize += moduleVirtualMemSize[moduleId];
		totalPhysicalMemSize += modulePhysicalMemSize[moduleId];

		if(moduleVirtualMemSize[moduleId] != 0 || modulePhysicalMemSize[moduleId] != 0)
		{
#if DEBUG_DRAW
			
			grcDebugDraw::AddDebugOutputEx(false, (color?Color_white:Color_BlueViolet), "%16s %8dK %8dK\n",
				pModule->GetModuleName(),
				moduleVirtualMemSize[moduleId]>>10, modulePhysicalMemSize[moduleId]>>10);
#endif // DEBUG_DRAW
			s32 v,p;
			GetModuleMemoryUsage(moduleId, v, p);
			streamAssertf(pModule->UsesExtraMemory() || (v==moduleVirtualMemSize[moduleId] && p == modulePhysicalMemSize[moduleId]),
				"Memory usage values for %s are incorrect (%d vs %d, %d vs %d)", 
				pModule->GetModuleName(),
				v, moduleVirtualMemSize[moduleId], p, modulePhysicalMemSize[moduleId]);
			if(bPrintf)
			{
				streamDisplayf("%s %dK %dK", moduleMgr.GetModule(moduleId)->GetModuleName(),
					moduleVirtualMemSize[moduleId]>>10, modulePhysicalMemSize[moduleId]>>10);
			}

			color = !color;
		}

	}

#if DEBUG_DRAW
	if (!bPrintf)
	{
		grcDebugDraw::AddDebugOutputEx(false, (color?Color_white:Color_BlueViolet), "%16s %8dK %8dK",
			"TOTAL",
			totalVirtualMemSize>>10, totalPhysicalMemSize>>10
			);
		color = !color;
	}
#endif // DEBUG_DRAW
}

void CStreamingDebug::ComputeModuleMemoryStats(ModuleMemoryStats *memStats, bool bShowTemporaryMemory)
{
	strStreamingModuleMgr& moduleMgr = strStreamingEngine::GetInfo().GetModuleMgr();
	s32 numModules = moduleMgr.GetNumberOfModules();
	s32 moduleId;


	// clear arrays
	memset(memStats->moduleVirtualMemSize, 0, sizeof(s32) * numModules);
	memset(memStats->modulePhysicalMemSize, 0, sizeof(s32) * numModules);
	memset(memStats->moduleRequiredVirtualMemSize, 0, sizeof(s32) * numModules);
	memset(memStats->moduleRequiredPhysicalMemSize, 0, sizeof(s32) * numModules);
	memset(memStats->moduleRequestedVirtualMemSize, 0, sizeof(s32) * numModules);
	memset(memStats->moduleRequestedPhysicalMemSize, 0, sizeof(s32) * numModules);

	// count up memory for each module while ignoring extra loaders
	for(moduleId=0; moduleId<numModules; moduleId++)
	{
		strStreamingModule* pModule = moduleMgr.GetModule(moduleId);
		// add up memory
		for(s32 obj = 0; obj<pModule->GetCount(); obj++)
		{
			strStreamingInfo* info = pModule->GetStreamingInfo(strLocalIndex(obj));
		
			bool measured = false;
			if (pModule->IsObjectInImage(strLocalIndex(obj)))
			{
				if (bShowTemporaryMemory || !pModule->RequiresTempMemory(strLocalIndex(obj)))
				{
					measured = true;
					int virtualSize, physicalSize;
					GetSizesSafe(pModule->GetStreamingIndex(strLocalIndex(obj)), *(pModule->GetStreamingInfo(strLocalIndex(obj))), virtualSize, physicalSize, false);

					if (pModule->HasObjectLoaded(strLocalIndex(obj)))
					{
						memStats->moduleVirtualMemSize[moduleId] += virtualSize;
						memStats->modulePhysicalMemSize[moduleId] += physicalSize;

						if (pModule->IsObjectRequired(strLocalIndex(obj)) || pModule->GetNumRefs(strLocalIndex(obj)) ||info->GetDependentCount())
						{
							memStats->moduleRequiredVirtualMemSize[moduleId] += virtualSize;
							memStats->moduleRequiredPhysicalMemSize[moduleId] += physicalSize;
						}
					}

					if (pModule->IsObjectRequested(strLocalIndex(obj)))
					{
						memStats->moduleRequestedVirtualMemSize[moduleId] += virtualSize;
						memStats->moduleRequestedPhysicalMemSize[moduleId] += physicalSize;
					}
				} 
				
				// if the module wasn't already counted (because it uses temp memory) just add the extra memory
				if (!measured && pModule->UsesExtraMemory())
				{
					int virtualSize, physicalSize;
					virtualSize = (int)pModule->GetExtraVirtualMemory(strLocalIndex(obj));
					physicalSize = (int)pModule->GetExtraPhysicalMemory(strLocalIndex(obj));

					if (pModule->HasObjectLoaded(strLocalIndex(obj)))
					{
						memStats->moduleVirtualMemSize[moduleId] += virtualSize;
						memStats->modulePhysicalMemSize[moduleId] += physicalSize;

						if (pModule->IsObjectRequired(strLocalIndex(obj)) || pModule->GetNumRefs(strLocalIndex(obj)) ||info->GetDependentCount())
						{
							memStats->moduleRequiredVirtualMemSize[moduleId] += virtualSize;
							memStats->moduleRequiredPhysicalMemSize[moduleId] += physicalSize;
						}
					}

					if (pModule->IsObjectRequested(strLocalIndex(obj)))
					{
						memStats->moduleRequestedVirtualMemSize[moduleId] += virtualSize;
						memStats->moduleRequestedPhysicalMemSize[moduleId] += physicalSize;
					}
				}	
			}
		}
	}
}
	
void CStreamingDebug::DisplayModuleMemoryDetailed(bool DEBUG_DRAW_ONLY(bPrintf), bool DEBUG_DRAW_ONLY(bOnlyShowArt), bool DEBUG_DRAW_ONLY(bShowTemporaryMemory))
{
#if DEBUG_DRAW
	strStreamingModuleMgr& moduleMgr = strStreamingEngine::GetInfo().GetModuleMgr();
	s32 numModules = moduleMgr.GetNumberOfModules();
	s32 moduleId;

	ModuleMemoryStats memStats;
	INIT_MODULE_MEMORY_STATS(&memStats, numModules);
	ComputeModuleMemoryStats(&memStats, bShowTemporaryMemory);

	bool color = true;

	if ((u32) s_MemoryProfileCompareLocation <= s_MemoryProfileLocations.m_Locations.GetCount())
	{
		char line[256];
		formatf(line, "Comparing the current scene to %s", s_MemoryProfileLocations.m_Locations[s_MemoryProfileCompareLocation].m_Name.c_str());

		if(bPrintf)
		{
			streamDisplayf("%s", line);
		}
		else
		{
			grcDebugDraw::AddDebugOutputEx(false, line);
		}
	}

	if(bPrintf)
	{
		streamDisplayf("\tName\tVirt Loaded\tPhys Loaded\tVirt Needed\tPhys Needed\tVirt Request\tPhys Request");
	}
	else
	{
		grcDebugDraw::AddDebugOutputEx(false,
			" Name            Loaded (Main/Vram)  | Needed (M/V)        + Requested (M/V)     = Scene (M/V)");
	}

	
	s32 totalVirtualMemSize = 0;
	s32 totalPhysicalMemSize = 0;
	s32 totalRequiredVirtualMemSize = 0;
	s32 totalRequiredPhysicalMemSize = 0;
	s32 totalRequestedVirtualMemSize = 0;
	s32 totalRequestedPhysicalMemSize = 0;
	for(moduleId = 0; moduleId < numModules; moduleId++)
	{
		strStreamingModule* pModule = moduleMgr.GetModule(moduleId);

		if (bOnlyShowArt)
		{
			if (pModule->GetStreamingModuleId() != g_TxdStore.GetStreamingModuleId()		&&
				pModule->GetStreamingModuleId() != g_DwdStore.GetStreamingModuleId()		&&
				pModule->GetStreamingModuleId() != g_DrawableStore.GetStreamingModuleId()	&&
				pModule->GetStreamingModuleId() != g_FragmentStore.GetStreamingModuleId())
			{
				continue; 
			}
		}

		totalVirtualMemSize += memStats.moduleVirtualMemSize[moduleId];
		totalPhysicalMemSize += memStats.modulePhysicalMemSize[moduleId];
		totalRequiredVirtualMemSize += memStats.moduleRequiredVirtualMemSize[moduleId];
		totalRequiredPhysicalMemSize += memStats.moduleRequiredPhysicalMemSize[moduleId];
		totalRequestedVirtualMemSize += memStats.moduleRequestedVirtualMemSize[moduleId];
		totalRequestedPhysicalMemSize += memStats.moduleRequestedPhysicalMemSize[moduleId];

		if(memStats.moduleVirtualMemSize[moduleId] != 0 || memStats.modulePhysicalMemSize[moduleId] != 0)
		{
			

			if(bPrintf)
			{
				streamDisplayf("\t%s\t%d\t%d\t%d\t%d\t%d\t%d",
					pModule->GetModuleName(),
					memStats.moduleVirtualMemSize[moduleId], memStats.modulePhysicalMemSize[moduleId],
					memStats.moduleRequiredVirtualMemSize[moduleId], memStats.moduleRequiredPhysicalMemSize[moduleId],
					memStats.moduleRequestedVirtualMemSize[moduleId], memStats.moduleRequestedPhysicalMemSize[moduleId]
				);
			}
			else
			{
				s32 sceneVirtual = (memStats.moduleRequiredVirtualMemSize[moduleId] + memStats.moduleRequestedVirtualMemSize[moduleId]) >> 10;
				s32 scenePhysical = (memStats.moduleRequiredPhysicalMemSize[moduleId] + memStats.moduleRequestedPhysicalMemSize[moduleId])>> 10;
				s32 diffVirtual = sceneVirtual - (s32)pModule->GetExpectedVirtualSize();
				s32 diffPhysical = scenePhysical - (s32)pModule->GetExpectedPhysicalSize();

				grcDebugDraw::AddDebugOutputEx(false, (color?Color_white:Color_BlueViolet),"%16s %8dK %8dK | %8dK %8dK + %8dK %8dK = %8dK %8dK - %8dK %8dK",
					pModule->GetModuleName(),
					memStats.moduleVirtualMemSize[moduleId]>>10, memStats.modulePhysicalMemSize[moduleId]>>10,
					memStats.moduleRequiredVirtualMemSize[moduleId]>>10, memStats.moduleRequiredPhysicalMemSize[moduleId]>>10,
					memStats.moduleRequestedVirtualMemSize[moduleId]>>10, memStats.moduleRequestedPhysicalMemSize[moduleId]>>10,
					sceneVirtual, 
					scenePhysical,
					diffVirtual,
					diffPhysical
					);
				color = !color;
			}

			s32 v,p;
			GetModuleMemoryUsage(moduleId, v, p);
			streamAssertf(pModule->UsesExtraMemory() || (v==memStats.moduleVirtualMemSize[moduleId] && p == memStats.modulePhysicalMemSize[moduleId]), "Memory usage values are incorrect");
		}

	}

	if( !bPrintf )
	{
		grcDebugDraw::AddDebugOutputEx(false, (color?Color_white:Color_BlueViolet), "%16s %8dK %8dK | %8dK %8dK + %8dK %8dK = %8dK %8dK",
			"TOTAL",
					totalVirtualMemSize>>10, totalPhysicalMemSize>>10,
					totalRequiredVirtualMemSize>>10, totalRequiredPhysicalMemSize>>10,
					totalRequestedVirtualMemSize>>10, totalRequestedPhysicalMemSize>>10,
					(totalRequiredVirtualMemSize + totalRequestedVirtualMemSize)>>10, 
					(totalRequiredPhysicalMemSize + totalRequestedPhysicalMemSize)>>10
					);
	}
#endif // DEBUG_DRAW
}


void CStreamingDebug::PrintResourceHeapPages()
{
	sysMemDistribution distV;
	sysMemAllocator::GetCurrent().GetAllocator(MEMTYPE_RESOURCE_VIRTUAL)->GetMemoryDistribution(distV);
#if !ONE_STREAMING_HEAP
	sysMemDistribution distP;
	sysMemAllocator::GetCurrent().GetAllocator(MEMTYPE_RESOURCE_PHYSICAL)->GetMemoryDistribution(distP);
#endif // !ONE_STREAMING_HEAP

#if ONE_STREAMING_HEAP		
		streamDisplayf("Page Size,Used/Free Virtual");
#else
		streamDisplayf("Page Size,Used/Free Virtual,U/F Phys");
#endif // ONE_STREAMING_HEAP
		
	for (int s=12; s<32; s++) 
	{
#if !ONE_STREAMING_HEAP
		streamDisplayf("%5dk,%5u/%-5u,%5u/%-5u", 1<<(s-10),distV.UsedBySize[s],distV.FreeBySize[s],distP.UsedBySize[s],distP.FreeBySize[s]);
#else // ONE_STREAMING_HEAP
		streamDisplayf("%5dk,%5u/%-5u", 1<<(s-10),distV.UsedBySize[s],distV.FreeBySize[s]);
#endif // !ONE_STREAMING_HEAP
	}
}

void CStreamingDebug::PrintMemoryMap()
{
	sysMemAllocator* pAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL);

#if !ONE_STREAMING_HEAP
	streamDisplayf("Game Virtual %dK (%dK -> %dK)",
		(s32)pAllocator->GetMemoryUsed()/1024,
		(s32)(pAllocator->GetMemoryAvailable() + pAllocator->GetMemoryUsed())/1024,
		(s32)pAllocator->GetMemoryAvailable()/1024);
	pAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_VIRTUAL);
	streamDisplayf("Resource Virtual %dK (%dK -> %dK)",
		(s32)pAllocator->GetMemoryUsed()/1024,
		(s32)(pAllocator->GetMemoryAvailable() + pAllocator->GetMemoryUsed())/1024,
		(s32)pAllocator->GetMemoryAvailable()/1024);
	pAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_PHYSICAL);
	streamDisplayf("Game Physical %dK (%dK -> %dK)",
		(s32)pAllocator->GetMemoryUsed()/1024,
		(s32)(pAllocator->GetMemoryAvailable() + pAllocator->GetMemoryUsed())/1024,
		(s32)pAllocator->GetMemoryAvailable()/1024);
	pAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_PHYSICAL);
	streamDisplayf("Resource Physical %dK (%dK -> %dK)",
		(s32)pAllocator->GetMemoryUsed()/1024,
		(s32)(pAllocator->GetMemoryAvailable() + pAllocator->GetMemoryUsed())/1024,
		(s32)pAllocator->GetMemoryAvailable()/1024);
#else
	streamDisplayf("Game %dK (%dK -> %dK)",
		(s32)pAllocator->GetMemoryUsed()/1024,
		(s32)(pAllocator->GetMemoryAvailable() + pAllocator->GetMemoryUsed())/1024,
		(s32)pAllocator->GetMemoryAvailable()/1024);
	pAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_VIRTUAL);
	streamDisplayf("Physical %dK (%dK -> %dK)",
		(s32)pAllocator->GetMemoryUsed()/1024,
		(s32)(pAllocator->GetMemoryAvailable() + pAllocator->GetMemoryUsed())/1024,
		(s32)pAllocator->GetMemoryAvailable()/1024);
#endif

#if ENABLE_DEBUG_HEAP
	pAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_DEBUG_VIRTUAL);
	streamDisplayf("Debug %dK (%dK -> %dK)",
		(s32)pAllocator->GetMemoryUsed()/1024,
		(s32)(pAllocator->GetMemoryAvailable() + pAllocator->GetMemoryUsed())/1024,
		(s32)pAllocator->GetMemoryAvailable()/1024);
#endif
}

//
// name:		CStreamingDebug::SwapBuffers()
// description:	Called when the render thread is blocked
void CStreamingDebug::SwapBuffers()
{
#if __BANK
    DEBUGSTREAMGRAPH.SwapBuffers();
#endif
}

#if __BANK

s32 OrderByName(strIndex i, strIndex j)
{
	char achName1[RAGE_MAX_PATH];
	char achName2[RAGE_MAX_PATH];
	const char *name1 = strStreamingEngine::GetInfo().GetObjectName(i, achName1, RAGE_MAX_PATH);
	const char *name2 = strStreamingEngine::GetInfo().GetObjectName(j, achName2, RAGE_MAX_PATH);
	return (stricmp(name1, name2) < 0);
}

//
// name:		OrderByMemorySize
// description:	Function used in sort to order objects by memory size
s32 OrderByMemorySize(strIndex i, strIndex j)
{
	int iPhySize, iVirtSize;
	int jPhySize, jVirtSize;
	GetSizesSafe(i, *strStreamingEngine::GetInfo().GetStreamingInfo(i), iVirtSize, iPhySize, s_DumpIncludeTempMemory);
	GetSizesSafe(j, *strStreamingEngine::GetInfo().GetStreamingInfo(j), jVirtSize, jPhySize, s_DumpIncludeTempMemory);
	return ((jVirtSize+jPhySize) - (iVirtSize+iPhySize) < 0);
}

s32 OrderByVirtualMemorySize(strIndex i, strIndex j)
{
	int iPhySize, iVirtSize;
	int jPhySize, jVirtSize;
	GetSizesSafe(i, *strStreamingEngine::GetInfo().GetStreamingInfo(i), iVirtSize, iPhySize, s_DumpIncludeTempMemory);
	GetSizesSafe(j, *strStreamingEngine::GetInfo().GetStreamingInfo(j), jVirtSize, jPhySize, s_DumpIncludeTempMemory);
	return ((jVirtSize) - (iVirtSize) < 0);
}

s32 OrderByPhysicalMemorySize(strIndex i, strIndex j)
{
	int iPhySize, iVirtSize;
	int jPhySize, jVirtSize;
	GetSizesSafe(i, *strStreamingEngine::GetInfo().GetStreamingInfo(i), iVirtSize, iPhySize, s_DumpIncludeTempMemory);
	GetSizesSafe(j, *strStreamingEngine::GetInfo().GetStreamingInfo(j), jVirtSize, jPhySize, s_DumpIncludeTempMemory);
	return ((jPhySize) - (iPhySize) < 0);
}

// fixme - KS
//s32 OrderByVirtualChunkSize(s32 i, s32 j)
//{
//	strStreamingInfo& info_i = *strStreamingEngine::GetInfo().GetStreamingInfo(i);
//	strStreamingInfo& info_j = *strStreamingEngine::GetInfo().GetStreamingInfo(j);
//
//	s32 i_size = info_i.IsFlagSet(STRFLAG_INTERNAL_RESOURCE)?info_i.GetResourceInfo().GetVirtualChunkSize():info_i.ComputeVirtualSize();
//	s32 j_size = info_j.IsFlagSet(STRFLAG_INTERNAL_RESOURCE)?info_j.GetResourceInfo().GetVirtualChunkSize():info_j.ComputeVirtualSize();
//
//	return ((j_size - i_size) < 0);
//}

//s32 OrderByPhysicalChunkSize(strIndex i, strIndex j)
//{
//	strStreamingInfo& info_i = *strStreamingEngine::GetInfo().GetStreamingInfo(i);
//	strStreamingInfo& info_j = *strStreamingEngine::GetInfo().GetStreamingInfo(j);
//
//	s32 i_size = info_i.IsFlagSet(STRFLAG_INTERNAL_RESOURCE)?info_i.GetResourceInfo().GetPhysicalChunkSize():info_i.ComputePhysicalSize();
//	s32 j_size = info_j.IsFlagSet(STRFLAG_INTERNAL_RESOURCE)?info_j.GetResourceInfo().GetPhysicalChunkSize():info_j.ComputePhysicalSize();
//
//	return ((j_size - i_size) < 0);
//}

//s32 OrderByChunkSize(strIndex i, strIndex j)
//{
//	strStreamingInfo& info_i = *strStreamingEngine::GetInfo().GetStreamingInfo(i);
//	strStreamingInfo& info_j = *strStreamingEngine::GetInfo().GetStreamingInfo(j);
//
//	s32 i_size = info_i.IsFlagSet(STRFLAG_INTERNAL_RESOURCE)?
//		MAX(info_i.GetResourceInfo().GetVirtualChunkSize(),info_i.GetResourceInfo().GetPhysicalChunkSize()):
//		MAX(info_i.ComputeVirtualSize(),info_i.ComputePhysicalSize());
//
//	s32 j_size = info_j.IsFlagSet(STRFLAG_INTERNAL_RESOURCE)?
//		MAX(info_j.GetResourceInfo().GetVirtualChunkSize(),info_j.GetResourceInfo().GetPhysicalChunkSize()):
//		MAX(info_j.ComputeVirtualSize(),info_j.ComputePhysicalSize());
//
//	return ((j_size - i_size) < 0);
//}

s32 OrderByNumRefs(strIndex i, strIndex j)
{
	strStreamingModule* pModuleI = strStreamingEngine::GetInfo().GetModule(i);
	strStreamingModule* pModuleJ = strStreamingEngine::GetInfo().GetModule(j);
	s32 refI = pModuleI->GetNumRefs(pModuleI->GetObjectIndex(i));
	s32 refJ = pModuleJ->GetNumRefs(pModuleJ->GetObjectIndex(j));

	return ((refI - refJ) < 0);
}

s32 OrderByLodDistance(strIndex i, strIndex j)
{
	float dist_i = 0.0f;
	float dist_j = 0.0f;
//	strStreamingInfo& info_i = *strStreamingEngine::GetInfo().GetStreamingInfo(i);
//	strStreamingInfo& info_j = *strStreamingEngine::GetInfo().GetStreamingInfo(j);
	s32 objIndex_i = strStreamingEngine::GetInfo().GetModule(i)->GetObjectIndex(i).Get();
	s32 objIndex_j = strStreamingEngine::GetInfo().GetModule(j)->GetObjectIndex(j).Get();
	u8 module_i = strStreamingEngine::GetInfo().GetModuleMgr().GetModuleIdFromIndex(i);
	u8 module_j = strStreamingEngine::GetInfo().GetModuleMgr().GetModuleIdFromIndex(j);
	if(module_i == CModelInfo::GetStreamingModuleId() || module_i == g_DrawableStore.GetStreamingModuleId())
	{
		CBaseModelInfo* pModelInfo = CModelInfo::GetModelInfoFromLocalIndex(objIndex_i);
		dist_i = (pModelInfo) ? pModelInfo->GetLodDistance() : 99999.0f;
	}
	if(module_j == CModelInfo::GetStreamingModuleId() || module_j == g_DrawableStore.GetStreamingModuleId())
	{
		CBaseModelInfo* pModelInfo = CModelInfo::GetModelInfoFromLocalIndex(objIndex_j);
		dist_j = (pModelInfo) ? pModelInfo->GetLodDistance() : 99999.0f;
	}

	return ((dist_j - dist_i) < 0.0f);
}

s32 OrderByFlags(strIndex i, strIndex j)
{
	strStreamingInfo& info_i = *strStreamingEngine::GetInfo().GetStreamingInfo(i);
	strStreamingInfo& info_j = *strStreamingEngine::GetInfo().GetStreamingInfo(j);

	return (((info_j.GetFlags()&STR_DONTDELETE_MASK) - (info_i.GetFlags()&STR_DONTDELETE_MASK)) < 0);
}

s32 OrderByImg(strIndex i, strIndex j)
{
	strStreamingInfo& info_i = *strStreamingEngine::GetInfo().GetStreamingInfo(i);
	strStreamingInfo& info_j = *strStreamingEngine::GetInfo().GetStreamingInfo(j);

	return ((fiCollection::GetCollectionIndex(info_i.GetHandle()) - fiCollection::GetCollectionIndex((info_j.GetHandle()))) < 0.0f);
}

s32 OrderByLSN(strIndex i, strIndex j)
{
	strStreamingInfo& info_i = *strStreamingEngine::GetInfo().GetStreamingInfo(i);
	strStreamingInfo& info_j = *strStreamingEngine::GetInfo().GetStreamingInfo(j);

	// Priority comes first!
	bool isPriority_i = (info_i.GetFlags() & STRFLAG_PRIORITY_LOAD) != 0;
	bool isPriority_j = (info_j.GetFlags() & STRFLAG_PRIORITY_LOAD) != 0;

	if (isPriority_i != isPriority_j)
	{
		return isPriority_i;
	}

	// NOTE that we use u32 here on purpose - if the file is behind the head position,
	// it will be a really large number and appear AFTER all the others.
	u32 lsn1 = info_i.GetCdPosn(false) - s_CurrentHeadPos;
	u32 lsn2 = info_j.GetCdPosn(false) - s_CurrentHeadPos;

	return lsn1 < lsn2;
}


#endif // __BANK

#if !__FINAL


void CStreamingDebug::GetMemoryStats(atArray<u32>& modelinfoIds, s32 &totalMemory, s32 &drawableMemory, s32 &textureMemory, s32 &boundsMemory)
{
	(void)modelinfoIds;
	(void)totalMemory;
	(void)drawableMemory;
	(void)textureMemory;
	(void)boundsMemory;
#if USE_PAGED_POOLS_FOR_STREAMING
	// TODO.
#else // USE_PAGED_POOLS_FOR_STREAMING
	std::set<strIndex> loadedData;

	totalMemory = 0;
	drawableMemory = 0;
	textureMemory = 0;
	boundsMemory = 0;
	
	strIndex backingStore[STREAMING_MAX_DEPENDENCIES];
	atUserArray<strIndex> deps(backingStore, STREAMING_MAX_DEPENDENCIES);

	for(s32 i=0; i<modelinfoIds.GetCount(); i++)
	{
		streamAssertf(modelinfoIds[i] != -1, "Array entry %d is invalid", i);
		fwModelId modelId((strLocalIndex(modelinfoIds[i])));
		strLocalIndex localIndex = CModelInfo::LookupLocalIndex(modelId);
		strIndex index = CModelInfo::GetStreamingModule()->GetStreamingIndex(localIndex);
		loadedData.insert(index);

		deps.ResetCount();
		CModelInfo::GetObjectAndDependencies(modelId, deps, NULL, 0);
		for(s32 j=0; j<deps.GetCount(); j++)
		{
			loadedData.insert(deps[j]);
		}
	}

	std::set<strIndex>::iterator iData;

	for(iData=loadedData.begin(); iData!=loadedData.end(); iData++)
	{
		strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModule(*iData);
		strStreamingInfo* pInfo = strStreamingEngine::GetInfo().GetStreamingInfo(*iData);
		int virtSize, physSize;
		GetSizesSafe(*iData, *pInfo, virtSize, physSize, false);

		s32 memory = virtSize + physSize;

		if(pModule == g_DrawableStore.GetStreamingModule() || pModule == g_FragmentStore.GetStreamingModule() || pModule == g_DwdStore.GetStreamingModule())
		{
			drawableMemory += memory;
		}
		else if(pModule == g_TxdStore.GetStreamingModule())
		{
			textureMemory += memory;
		}
		else if(pModule == fwAnimManager::GetStreamingModule())
		{
			//textureMemory += memory;
		}
	}
	totalMemory = drawableMemory + textureMemory + boundsMemory;
#endif // USE_PAGED_POOLS_FOR_STREAMING
}

#if !__FINAL
void CStreamingDebug::GetMemoryStatsByAllocation(atArray<u32>& modelinfoIds, s32 &totalPhysicalMemory, s32 &totalVirtualMemory, s32 &drawablePhysicalMemory, s32 &drawableVirtualMemory, s32 &texturePhysicalMemory, s32 &textureVirtualMemory, s32 &boundsPhysicalMemory, s32 &boundsVirtualMemory)
{
	USE_DEBUG_MEMORY();

	totalPhysicalMemory = 0;
	drawablePhysicalMemory = 0;
	texturePhysicalMemory = 0;
	boundsPhysicalMemory = 0;

	totalVirtualMemory = 0;
	drawableVirtualMemory = 0;
	textureVirtualMemory = 0;
	boundsVirtualMemory = 0;

	strIndex deps[STREAMING_MAX_DEPENDENCIES];

	atSimplePooledMapType<strIndex, u32> loadedDataMapPool;
	loadedDataMapPool.Init(4096);

	for(s32 i=0; i<modelinfoIds.GetCount(); i++)
	{
		fwModelId modelId((strLocalIndex(modelinfoIds[i])));
		strLocalIndex localIndex = CModelInfo::LookupLocalIndex(modelId);
		strIndex index = CModelInfo::GetStreamingModule()->GetStreamingIndex(localIndex);

		u32 * pResult = loadedDataMapPool.m_Map.Access(index);
		if(!pResult)
		{
			loadedDataMapPool.m_Map.Insert(index, 1);
			s32 numDeps = CModelInfo::GetStreamingModule()->GetDependencies(localIndex, &deps[0], STREAMING_MAX_DEPENDENCIES);
			for(s32 j=0; j<numDeps; j++)
			{
				index = deps[j];
				pResult = loadedDataMapPool.m_Map.Access(index);
				if(!pResult)
				{
					loadedDataMapPool.m_Map.Insert(index, 1);
				}
			}
		}
	}

	for(int i=0;i<loadedDataMapPool.m_Map.GetNumSlots();i++)
	{
		rage::atMapEntry<strIndex, u32> *pEntry = loadedDataMapPool.m_Map.GetEntry(i);
		while (pEntry)
		{
			strIndex index = pEntry->key;

			strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModule(index);
			strStreamingInfo* pInfo = strStreamingEngine::GetInfo().GetStreamingInfo(index);

			int virtMemory, physMemory;
			GetSizesSafe(index, *pInfo, virtMemory, physMemory, false);

			if(pModule == CModelInfo::GetStreamingModule() || pModule == g_DwdStore.GetStreamingModule())
			{
				drawablePhysicalMemory += physMemory;
				drawableVirtualMemory += virtMemory;
			}
			else if(pModule == g_TxdStore.GetStreamingModule())
			{
				texturePhysicalMemory += physMemory;
				textureVirtualMemory += virtMemory;
			}
			else if(pModule == fwAnimManager::GetStreamingModule())
			{
				//textureMemory += memory;
			}


			pEntry = pEntry->next;
		}
	}

	totalPhysicalMemory = drawablePhysicalMemory + texturePhysicalMemory + boundsPhysicalMemory;
	totalVirtualMemory = drawableVirtualMemory + textureVirtualMemory + boundsVirtualMemory;

	loadedDataMapPool.Reset();
	loadedDataMapPool.m_Pool.Destroy();
}
#endif // !__FINAL

#endif // !__FINAL

#if __BANK

void CStreamingDebug::PrintMemoryDistribution(const sysMemDistribution& distribution, char* header, char* usedMemory, char* freeMemory)
{
	static const char* memory[4] = {"%6dB", "%5dKB", "%5dMB", "%5dGB"};
	strcpy(header, "size ");
	strcpy(usedMemory, "used ");
	strcpy(freeMemory, "free ");
	char tmp[32];

	for (int i = 12; i < 26; ++i)
	{
		size_t size = (1 << i);

		int memoryIndex = 0;
		while (size > 1024)
		{
			memoryIndex++;
			size >>= 10;
		}
		sprintf(tmp, memory[memoryIndex], size);
		strcat(header, tmp);
		sprintf(tmp, "%7d", distribution.UsedBySize[i]);
		strcat(usedMemory, tmp);
		sprintf(tmp, "%7d", distribution.FreeBySize[i]);
		strcat(freeMemory, tmp);
	}
}

void CStreamingDebug::PrintCompactResourceHeapPages(char *buffer, int size)
{
	sysMemDistribution distV;
	sysMemAllocator::GetCurrent().GetAllocator(MEMTYPE_RESOURCE_VIRTUAL)->GetMemoryDistribution(distV);
#if !ONE_STREAMING_HEAP
	sysMemDistribution distP;
	sysMemAllocator::GetCurrent().GetAllocator(MEMTYPE_RESOURCE_PHYSICAL)->GetMemoryDistribution(distP);
#endif // !ONE_STREAMING_HEAP

	safecpy(buffer, "Available: ", size);

	// Two passes - pass 0 for virtual, pass 1 for physical (on !ONE_STREAMING_HEAP).
	for (int pass=0; pass<1 + !ONE_STREAMING_HEAP; pass++)
	{
		bool isVirtual = (pass == 0);
		bool first = true;

		for (int s=12; s<24; s++) 
		{
#if !ONE_STREAMING_HEAP
			int freePageCount = (isVirtual) ? distV.FreeBySize[s] : distP.FreeBySize[s];
#else // ONE_STREAMING_HEAP
			int freePageCount = distV.FreeBySize[s];
#endif // !ONE_STREAMING_HEAP

			if (freePageCount)
			{
				char smallBuffer[32];

				if (first)
				{
					safecat(buffer, (isVirtual) ? "M: " : "V: ", size);
					first = false;
				}

				size_t pageSize = pgRscBuilder::ComputeLeafSize(1 << s, !isVirtual);
				formatf(smallBuffer, "%dx%dK ", freePageCount, pageSize / 1024);
				safecat(buffer, smallBuffer, size);
			}
		}
	}
}

//
// name:		DisplayObjectDependencies
// description:	Display the objects an object is dependent on
static void DisplayObjectDependencies(fiStream *stream, bool toScreen, strIndex obj)
{
	static s32 gTabs = 0;
	char spaces[17];

	gTabs++;
	if(gTabs > 8)
		streamErrorf("Too many tabs");

	// create spaces string
	for(s32 i=0; i<gTabs; i++)
	{
		spaces[i*2] = ' ';
		spaces[i*2+1] = ' ';
	}
	spaces[gTabs*2] = '\0';

	strIndex deps[STREAMING_MAX_DEPENDENCIES];
	strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModule(obj);
	s32 numDeps = pModule->GetDependencies(pModule->GetObjectIndex(obj), &deps[0], STREAMING_MAX_DEPENDENCIES);

	for(s32 j=0; j<numDeps; j++)
	{
		strStreamingInfo& info = *strStreamingEngine::GetInfo().GetStreamingInfo(deps[j]);
		strStreamingModule* pDepModule = strStreamingEngine::GetInfo().GetModule(deps[j]);
		strLocalIndex depObjIndex = pDepModule->GetObjectIndex(deps[j]);

		Color32 color = Color_grey90;
		if(pDepModule->HasObjectLoaded(depObjIndex))
			color = Color_red;

		int virtSize, physSize;
		GetSizesSafe(deps[j], info, virtSize, physSize, s_DumpIncludeTempMemory);

		char refString[16];
		pDepModule->GetRefCountString(depObjIndex, refString, sizeof(refString));

		char depString[16];
		formatf(depString, "Dep=%d", info.GetDependentCount());

		char flags[256];
		CreateFlagString(info.GetFlags(), flags, sizeof(flags));

		char line[256];
		formatf(line, "%s%48s|%8dK|%8dK|%s|%s|%s",
			spaces,
			strStreamingEngine::GetObjectName(deps[j]),
			virtSize>>10,
			physSize>>10,
			refString,
			depString,
			flags);

		CStreamingDebug::OutputLine(stream, toScreen, color, line);
		DisplayObjectDependencies(stream, toScreen, deps[j]);
	}

	gTabs--;
}

//
// name:		DisplayObjectDependents
// description:	Display the objects that are dependent on this obj
static void DisplayObjectDependents(fiStream *stream, bool toScreen, strIndex obj)
{
	strStreamingInfoIterator it;

	while (!it.IsAtEnd())
	{
		// ignore entries with no loader setup
		strIndex index = *it;
		++it;

		strStreamingInfo& info = *strStreamingEngine::GetInfo().GetStreamingInfo(index);
		if(!info.IsInImage())
			continue;

		strIndex deps[STREAMING_MAX_DEPENDENCIES];
		strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModule(index);
		s32 numDeps = pModule->GetDependencies(pModule->GetObjectIndex(index), &deps[0], STREAMING_MAX_DEPENDENCIES);
		for(s32 j=0; j<numDeps; j++)
		{
			if(deps[j] == obj)
			{
				strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModule(index);
				strLocalIndex objIndex = pModule->GetObjectIndex(index);

				Color32 color = Color_grey90;
				if(pModule->HasObjectLoaded(objIndex))
					color = Color_red;

				if (info.GetStatus() == STRINFO_LOADREQUESTED || info.GetStatus() == STRINFO_LOADING)
					color = Color_yellow;

				int virtSize, physSize;
				GetSizesSafe(index, info, virtSize, physSize, s_DumpIncludeTempMemory);

				char refString[16];
				pModule->GetRefCountString(objIndex, refString, sizeof(refString));

				char depString[16];
				formatf(depString, "Dep=%d", info.GetDependentCount());

				char flags[256];
				CreateFlagString(info.GetFlags(), flags, sizeof(flags));

				char line[256];
				formatf(line, "  %48s|%8dK|%8dK|%s|%s|%s",
					//spaces,
					strStreamingEngine::GetObjectName(index),
					virtSize>>10,
					physSize>>10,
					refString,
					depString,
					flags);

				CStreamingDebug::OutputLine(stream, toScreen, color, line);
			}
		}
	}
}

//
// name:		GetEntityList
// description:	Return list of entities that are using this object
static s32 GetEntityList(strIndex index, RegdEnt* ppEntityList, s32 max)
{
	strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModule(index);
	s32 objIndex = pModule->GetObjectIndex(index).Get();
	s32 count=0;
	// If this is a model index find all the loaded entities with this model index
	if(pModule == CModelInfo::GetStreamingModule())
	{
		const fwLinkList<ActiveEntity>& activeList = g_SceneStreamerMgr.GetStreamingLists().GetActiveList();

		fwLink<ActiveEntity>* pLastLink = activeList.GetLast()->GetPrevious();

		while(pLastLink != activeList.GetFirst())
		{
			CEntity* pEntity = static_cast<CEntity*>(pLastLink->item.GetEntity());
			pLastLink = pLastLink->GetPrevious();

			if (pEntity)
			{
				const int modelIndex = pEntity->GetModelIndex();

				if (modelIndex == objIndex)
				{
					ppEntityList[count++] = static_cast<CEntity*>(pEntity);
				}
			}
		}

		/*
        fwEntityContainer::Pool* pool = fwEntityContainer::GetPool();

        if (pool)
        {
            for (int i = 0; i < pool->GetSize() && count < max; i++)
            {
                fwEntityContainer* container = pool->GetSlot(i);

                if (container)
                {
                    atArray<fwEntity*> entities;
                    container->GetEntityArray(entities);

                    for (int j = 0; j < entities.size() && count < max; j++)
                    {
                        CEntity* entity = (CEntity*)entities[j];

                        if (entity)
                        {
                            streamAssert(entity->GetType() >= 0 && entity->GetType() < ENTITY_TYPE_TOTAL);
                            const int modelIndex = entity->GetModelIndex();
                            if (modelIndex == objIndex)
                            {
                                ppEntityList[count++] = static_cast<CEntity*>(entity);
                            }
                        }
                    }
                }
            }
        }
		*/
	}
	else if(strStreamingEngine::GetInfo().GetStreamingInfoRef(index).GetDependentCount() > 0)
	{

		// find all objects dependent on this object
		strStreamingInfoIterator it;

		while (!it.IsAtEnd())
		{
			strIndex infoIndex = *it;
			++it;

			// ignore entries with no loader setup
			if (!strStreamingEngine::GetInfo().GetStreamingInfo(infoIndex)->IsInImage())
				continue;

			strIndex deps[STREAMING_MAX_DEPENDENCIES];
			strStreamingModule* pDepModule = strStreamingEngine::GetInfo().GetModule(infoIndex);
			s32 numDeps = pDepModule->GetDependencies(pDepModule->GetObjectIndex(infoIndex), &deps[0], STREAMING_MAX_DEPENDENCIES);
			for(s32 j=0; j < numDeps && count < max; j++)
			{
				if(deps[j] == index)
				{
					s32 count2 = GetEntityList(infoIndex, ppEntityList+count, max-count);
					count += count2;
				}
			}
		}
	}
	return count;
}
#if __DEV

void CStreamingDebug::ShowBucketInfoForSelected()
{
	CEntity* pSelectedEntity = (CEntity*) g_PickerManager.GetSelectedEntity();
	if (pSelectedEntity)
	{
		// Find this entity in the buckets.
		for (int x=0; x<2; x++)
		{
			CStreamingBucketSet *bucketSet = (x == 0) ? 
				&(g_SceneStreamerMgr.GetStreamingLists().GetActiveEntityList().GetBucketSet()) :
				&(g_SceneStreamerMgr.GetStreamingLists().GetNeededEntityList().GetBucketSet());

			int count = bucketSet->GetEntryCount();
			const BucketEntry *entry = bucketSet->GetEntries();

			while (count--)
			{
				if (entry->GetEntity() == pSelectedEntity)
				{
					// Found it.
					grcDebugDraw::AddDebugOutput("%s is in %s list, score %f",
						pSelectedEntity->GetModelName(), (x == 0) ? "ACTIVE" : "NEEDED",
						entry->m_Score);
					return;
				}

				++entry;
			}
		}

		grcDebugDraw::AddDebugOutput("%s is not in any bucket set",	pSelectedEntity->GetModelName());
	}
}

//
// name:		CStreamingDebug::DisplayRequestedObjectsByModule
// description:	Display the requested object counts, grouped by module
void CStreamingDebug::DisplayRequestedObjectsByModule()
{
	grcDebugDraw::AddDebugOutputEx(false, "OPTICAL");
	DisplayRequestedObjectsByModuleFiltered(false);
	grcDebugDraw::AddDebugOutputEx(false, "HDD");
	DisplayRequestedObjectsByModuleFiltered(true);
}

//
// name:		CStreamingDebug::DisplayRequestedObjectsByModule
// description:	Display the requested object counts, grouped by module, with filters applied
void CStreamingDebug::DisplayRequestedObjectsByModuleFiltered(bool onHdd)
{
	const int MAX_LINE_LEN = 80;
	int moduleCount = strStreamingEngine::GetInfo().GetModuleMgr().GetNumberOfModules();

	char *moduleBuffer = Alloca(char, moduleCount * MAX_LINE_LEN);
	int *lineLen = Alloca(int, moduleCount);

	for (int x=0; x<moduleCount; x++)
	{
		moduleBuffer[x * MAX_LINE_LEN] = 0;
		lineLen[x] = 0;
	}

	RequestInfoList::Iterator it(strStreamingEngine::GetInfo().GetRequestInfoList()->GetFirst());

	while (it.IsValid())
	{		
		strStreamingInfo& info = strStreamingEngine::GetInfo().GetStreamingInfoRef(it.Item()->m_Index);

		bool isPriority = (info.GetFlags() & STRFLAG_PRIORITY_LOAD) != 0;
		bool isOnHdd = (info.GetDevices(it.Item()->m_Index) & (1 << pgStreamer::HARDDRIVE)) != 0;

		if (info.IsFlagSet(STRFLAG_INTERNAL_DUMMY) && bHideDummyRequestsOnHdd)
		{
			isOnHdd = false;
		}

		if (isOnHdd == onHdd)
		{
			u8 moduleIndex = strStreamingEngine::GetInfo().GetModuleMgr().GetModuleIdFromIndex(it.Item()->m_Index);
			streamAssert((u32) moduleIndex < moduleCount);
			int currentLineLen = lineLen[moduleIndex];

			if (currentLineLen < MAX_LINE_LEN - 2)
			{
				moduleBuffer[moduleIndex * MAX_LINE_LEN + currentLineLen] = (isPriority) ? 'X' : 'o';
				currentLineLen++;
				moduleBuffer[moduleIndex * MAX_LINE_LEN + currentLineLen] = 0;
				lineLen[moduleIndex] = currentLineLen;
			}
		}

		it.Next();
	}

	for (int x=0; x<moduleCount; x++)
	{
		strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(x);

		grcDebugDraw::AddDebugOutputEx(false, "%-30s: %s", pModule->GetModuleName(), &moduleBuffer[x * MAX_LINE_LEN]);
	}
}

void ResetLoadedCounts()
{
	strStreamingEngine::GetInfo().GetModuleMgr().ResetLoadedCounts();
}

//
// name: DisplayTotalLoadedObjectsByModule
// description: Displays all the objects loaded by module
void CStreamingDebug::DisplayTotalLoadedObjectsByModule()
{
	const int MAX_LINE_LEN = 80;
	static int multiple = 50;
	int moduleCount = strStreamingEngine::GetInfo().GetModuleMgr().GetNumberOfModules();

	for(int i=0; i<moduleCount; i++)
	{
		strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(i);
		char line[MAX_LINE_LEN];
		int number=0;

		int loadedCount = (pModule->GetLoadedCount()+multiple-1)/multiple;
		for(int j=0; j<loadedCount; j++)
		{
			if(number >= MAX_LINE_LEN-1)
				break;
			line[number++] = 'O';
		}
		line[number] = 0;
		grcDebugDraw::AddDebugOutputEx(false, "%-30s: %s", pModule->GetModuleName(), line);
	}
}
#endif // __DEV

//
// name:		CStreamingDebug::DisplayTxdsInMemory
// description:	Display all the txds in memory
void CStreamingDebug::DisplayObjectsInMemory(fiStream *stream, bool toScreen, s32 moduleId, const u32 fileStatusFilter, bool forceShowDummies)
{
	static const char *deviceNames[] = { "OPTICAL", "HARD DRIVE" };
	int firstModule, lastModule;

	if (moduleId < 0)
	{
		// All modules.
		firstModule = 0;
		lastModule = strStreamingEngine::GetInfo().GetModuleMgr().GetNumberOfModules();
	}
	else
	{
		// Just one module.
		firstModule = moduleId;
		lastModule = moduleId+1;
	}

	// IF we split by device, we need to do two passes. If not, we bail after the first iteration.
	for (int device=0; device<pgStreamer::DEVICE_COUNT; device++)
	{
		bool includeDummies = s_DumpIncludeDummies || forceShowDummies;
		bool requiredOnly = s_DumpRequiredOnly;
		bool includeTempMemory = s_DumpIncludeTempMemory;
		bool excludeScene = s_DumpExcludeSceneAssets;
		bool onlyScene = s_DumpOnlySceneAssets;
		bool onlyZone = s_DumpOnlyZone;
		bool onlyNoEntities = s_DumpOnlyNoEntities;
		bool addDependencyCost = s_DumpAddDependencyCost;
		bool immediateDependencyCostOnly = s_DumpImmediateDependencyCostOnly;
		bool cachedOnly = s_DumpCachedOnly;


		u32 deviceFilter = ~0U;

		if (s_DumpSplitByDevice)
		{
			deviceFilter = 1 << device;
			OutputLine(stream, toScreen, Color_white, deviceNames[device]);
		}

		strIndex* objInMemory;

		{
			USE_DEBUG_MEMORY();
			objInMemory = rage_new strIndex[strStreamingEngine::GetInfo().GetStreamingInfoSize()];
		}

		int numObjs = 0;
		for (int x=firstModule; x<lastModule; x++)
		{
			strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(x);

			for(s32 i=0; i<pModule->GetCount(); i++)
			{
				if (pModule->IsObjectInImage(strLocalIndex(i)))
				{
					strIndex strIndex = pModule->GetStreamingIndex(strLocalIndex(i));
					strStreamingInfo& info = *strStreamingEngine::GetInfo().GetStreamingInfo(strIndex);

					if (includeDummies || !info.IsFlagSet(STRFLAG_INTERNAL_DUMMY))
					{
						if (fileStatusFilter & (1 << info.GetStatus()))
						{
							if (!requiredOnly || strStreamingEngine::GetInfo().IsObjectRequired(strIndex) || pModule->GetNumRefs(strLocalIndex(i)) > 0)
							{
								if (deviceFilter & (1 << info.GetPreferredDevice(strIndex)))
								{
									if (!cachedOnly || (info.GetDependentCount() == 0 && pModule->GetNumRefs(strLocalIndex(i)) == 0 && strStreamingEngine::GetInfo().IsObjectDeletable(strIndex)))
									{
										if (!s_ModuleSubstringSearch[0] || (pModule->GetName(strLocalIndex(i)) && stristr(pModule->GetName(strLocalIndex(i)), s_ModuleSubstringSearch)))
										{
											if (!excludeScene || (s_SceneMap.Access(strIndex) == NULL))
											{
												if (!onlyScene || (s_SceneMap.Access(strIndex) != NULL))
												{
													if (!onlyZone || (info.GetFlags() & STRFLAG_ZONEDASSET_REQUIRED))
													{
														if (!onlyNoEntities || !HasEntities(strIndex))
														{
															bool bIncludeThis = true;
															if (bDisplayOnlyWithinMemRange)
															{
																int virtSize, physSize;
																GetSizesSafe(strIndex, info, virtSize, physSize, includeTempMemory);

																if (addDependencyCost)
																{
																	ComputeDependencyCost(strIndex, virtSize, physSize, !immediateDependencyCostOnly);
																}

																s32 totalKb = (virtSize + physSize) >> 10;
																bIncludeThis = (totalKb >= minMemSizeToDisplayKb && totalKb <= maxMemSizeToDisplayKb);
															}
															if (bIncludeThis)
															{
																objInMemory[numObjs++] = pModule->GetStreamingIndex(strLocalIndex(i));
															}
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}

		if (!s_DumpSplitByDevice)
		{
			if (gCurrentSelectedObj >= numObjs)
			{
				gCurrentSelectedObj = 0;
			}
		}

		DisplayObjectList(stream, toScreen, &objInMemory[0], numObjs);

		{
			USE_DEBUG_MEMORY();
			delete[] objInMemory;
		}

		// Don't do two passes if we don't split by device.
		if (!s_DumpSplitByDevice)
		{
			break;
		}
	}

}

//
// name:		CStreamingDebug::DisplayObjectList
// description:	Display a list of objects with streaming debug info
void CStreamingDebug::DisplayObjectList(fiStream *stream, bool toScreen, strIndex *pList, s32 numObjs)
{
	bool addDependencyCost = s_DumpAddDependencyCost;
	bool showEntities = s_DumpShowEntities;
	bool includeInactiveEntities = s_DumpIncludeInactiveEntities;
	bool immediateDependencyCostOnly = s_DumpImmediateDependencyCostOnly;

	atMap<strIndex, bool> dependencies;

	// Sort objects
	if (numObjs>1)
	{
		switch(s_eSortDisplayedObjects)
		{
		case SORT_NAME:
			std::sort(&pList[0], &pList[numObjs], &OrderByName);
			break;
		case SORT_SIZE:
		case SORT_CHUNKS_SIZE:
		case SORT_EXTRAINFO:
			std::sort(&pList[0], &pList[numObjs], &OrderByMemorySize);
			break;
		case SORT_VIRTUAL_SIZE:
			std::sort(&pList[0], &pList[numObjs], &OrderByVirtualMemorySize);
			break;
		case SORT_PHYSICAL_SIZE:
			std::sort(&pList[0], &pList[numObjs], &OrderByPhysicalMemorySize);
			break;
		case SORT_NUMREFS:
			std::sort(&pList[0], &pList[numObjs], &OrderByNumRefs);
			break;
		case SORT_LODDISTANCE:
			std::sort(&pList[0], &pList[numObjs], &OrderByLodDistance);
			break;
		case SORT_FLAGS:
			std::sort(&pList[0], &pList[numObjs], &OrderByFlags);
			break;
		case SORT_IMGFILE:
			std::sort(&pList[0], &pList[numObjs], &OrderByImg);
			break;
		case SORT_LSN:
			std::sort(&pList[0], &pList[numObjs], &OrderByLSN);
			break;
		case SORT_NONE:
		default:
			break;
		}
	}

	// Tally up total sizes
	int totalVirt = 0;
	int totalPhys = 0;

	for(s32 i=0; i<numObjs; i++)
	{
		strIndex index = pList[i];
		strStreamingInfo& info = *strStreamingEngine::GetInfo().GetStreamingInfo(index);

		int virtSize, physSize;
		GetSizesSafe(index, info, virtSize, physSize, s_DumpIncludeTempMemory);

		if (addDependencyCost)
		{
			AddDependencyCost(index, virtSize, physSize, dependencies, true);
		}

		totalVirt += virtSize>>10;
		totalPhys += physSize>>10;
	}

	char header[128];
	formatf(header, "Total object count: %d", numObjs);

	char headerLine[256];
	formatf(headerLine, "%-50s|%8dK|%8dK", header, totalVirt, totalPhys);
	OutputLine(stream, toScreen, Color_white, headerLine);

	for(s32 i=0; i<numObjs; i++)
	{
		char finalLine[256];

		strIndex index = pList[i];
		strStreamingInfo& info = *strStreamingEngine::GetInfo().GetStreamingInfo(index);
		strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModule(index);
		strLocalIndex objIndex = pModule->GetObjectIndex(index);

		char nameBuffer[256];
		const char* name = strStreamingEngine::GetInfo().GetObjectName(index, nameBuffer, sizeof(nameBuffer));

		char flags[256];
		CreateFlagString(info.GetFlags(), flags, sizeof(flags));

		char memory[256] = {0};
		int objVirtSize = info.ComputeOccupiedVirtualSize(index, true, s_DumpIncludeTempMemory);
		int objPhysSize = info.ComputePhysicalSize(index, true);

		if (addDependencyCost)
		{
			ComputeDependencyCost(index, objVirtSize, objPhysSize, !immediateDependencyCostOnly);
		}

		formatf(memory, "%8dK|%8dK", objVirtSize>>10, objPhysSize>>10);

		char refString[16];
		pModule->GetRefCountString(objIndex, refString, sizeof(refString));

		char depString[16];
		formatf(depString, "Dep=%d", info.GetDependentCount());

		Color32 color = Color_grey90;

		if (gCurrentSelectedObj == i)
			color = Color_red;

		// Highlight priority requests.
		if (info.IsFlagSet(STRFLAG_PRIORITY_LOAD))
		{
			color.SetBlue(0);
		}

		// Dummy requests are dimmer.
		if (info.IsFlagSet(STRFLAG_INTERNAL_DUMMY))
		{
			color = Color32(color.GetRGBA()*Vec4V(0.75f,0.75f,0.75f,1.0f));
		}

		int virtSize, physSize;
		GetSizesSafe(index, info, virtSize, physSize, s_DumpIncludeTempMemory);
		
		if(s_eSortDisplayedObjects == SORT_LODDISTANCE && pModule == CModelInfo::GetStreamingModule())
		{
			CBaseModelInfo* pModelInfo = CModelInfo::GetModelInfoFromLocalIndex(objIndex.Get());
			streamAssert(pModelInfo);

			formatf(finalLine, "(%05d) %50s|%s|%d|lod|%f",
				objIndex,
				name,
				memory,
				pModelInfo->GetLodDistanceUnscaled());
		}
		else if(s_eSortDisplayedObjects == SORT_CHUNKS_SIZE)
		{
			char needed[256] = {0};

			if (info.IsFlagSet(STRFLAG_INTERNAL_RESOURCE))
			{
				s32 imageIndex = strPackfileManager::GetImageFileIndexFromHandle(info.GetHandle()).Get();
				if (imageIndex != -1)
				{
					//strIndex archiveStrIndex = strPackfileManager::GetStreamingModule()->GetStreamingIndex(imageIndex);
					bool archiveIsResident = strPackfileManager::IsImageResident(imageIndex);
					if (archiveIsResident)
					{
						info.GetResourceInfo().Print(needed, 256);
					}
				}
			}

			formatf(finalLine, "(%05d) %50s|%s|%s|%s|%s",
				objIndex,
				name,
				memory,
				refString,
				needed,
				flags
				);
		}
		else if(s_eSortDisplayedObjects == SORT_FLAGS)
		{
			formatf(finalLine, "(%05d) %50s|%s|%s|%s", 
				objIndex,
				name,
				memory,
				refString,
				flags);
		}
		else if(s_eSortDisplayedObjects == SORT_IMGFILE)
		{
			const char *packfileName = strPackfileManager::GetImageFileNameFromHandle(info.GetHandle());
			formatf(finalLine, "(%05d) %50s|%s|%s|img|%s",
				objIndex,
				name,
				memory,
				refString,
				packfileName);
		}
		else if(s_eSortDisplayedObjects == SORT_EXTRAINFO)
		{
			char printExtra[256] = {0};
			pModule->PrintExtraInfo(objIndex, printExtra, sizeof(printExtra));

			formatf(finalLine, "(%05d) %50s|%s|%s|=> %s",
				objIndex,
				name,
				memory,
				refString,
				printExtra);
		}
		else
		{
			char dependencyString[256];

			if (bShowDependencies)
			{
				CreateDependencyString(index, bShowUnfulfilledDependencies, dependencyString, sizeof(dependencyString));
			}
			else
			{
				dependencyString[0] = 0;
			}

			const char *device = (info.GetPreferredDevice(index) == pgStreamer::OPTICAL) ? "ODD" : "HDD";
			
			char lsnString[16];
			
			if (s_eSortDisplayedObjects == SORT_LSN)
			{
				s32 lsn1 = info.GetCdPosn(false) - s_CurrentHeadPos;
				formatf(lsnString, "%10d", lsn1);
			}
			else
			{
				lsnString[0] = 0;
			}

			formatf(finalLine, "(%05d) %50s|%s|%s|%s|%s|%s|%s|%s",
				objIndex,
				name,
				memory,
				refString,
				depString,
				device,
				lsnString,
				flags,
				dependencyString
				);
		}

		OutputLine(stream, toScreen, color, finalLine);

		if(gCurrentSelectedObj == i)
		{
			if(gbSeeObjDependencies)
			{
				DisplayObjectDependencies(stream, toScreen, pList[i]);
			}
			if(gbSeeObjDependents)
			{
				DisplayObjectDependents(stream, toScreen, pList[i]);
			}

            if(gCurrentEntityListObj != pList[i])
            {
				if (bDrawBoundingBoxOnEntities)
				{
					s_entityListNum = GetEntityList(pList[i], &s_entityList[0], 256);
				}
				else
				{
					s_entityListNum = 0;
				}

                gCurrentEntityListObj = pList[i];
            }

			if (bDrawBoundingBoxOnEntities)
			{
				for(s32 j=0; j<s_entityListNum; j++)
				{
					if(s_entityList[j])
					{
						CDebugScene::DrawEntityBoundingBox(s_entityList[j], Color32(255,255,0));
						s_entityList[j]->SetIsVisibleForModule(SETISVISIBLE_MODULE_DEBUG, fwTimer::GetSystemFrameCount() & 1);
					}
				}
			}
#if __DEV
			UpdateObjectFraming();
#endif // __DEV
		}

		if (showEntities && pModule == CModelInfo::GetStreamingModule())
		{
			if (!includeInactiveEntities)
			{
				const fwLinkList<ActiveEntity>& activeList = g_SceneStreamerMgr.GetStreamingLists().GetActiveList();

				fwLink<ActiveEntity>* pLastLink = activeList.GetLast()->GetPrevious();

				while(pLastLink != activeList.GetFirst())
				{
					CEntity* pEntity = static_cast<CEntity*>(pLastLink->item.GetEntity());
					pLastLink = pLastLink->GetPrevious();

					if (pEntity)
					{
						const int modelIndex = pEntity->GetModelIndex();

						if (modelIndex == objIndex.Get())
						{
							Vec3V pos = pEntity->GetTransform().GetPosition();
							formatf(finalLine, "%50s|%8.2f|%8.2f|%8.2f|%s",
								name, pos.GetXf(), pos.GetYf(), pos.GetZf(), GetEntityTypeStr(pEntity->GetType()));

							OutputLine(stream, toScreen, Color_yellow, finalLine);
						}
					}
				}
			}
			else
			{
				// Find every entity using this archetype.
				for (int x=0; x<ENTITY_TYPE_TOTAL; x++)
				{
					fwBasePool *pool = CEntity::ms_EntityPools[x];

					if (pool)
					{
						int count = pool->GetSize();

						for (int y=0; y<count; y++)
						{
							CEntity *pEntity = (CEntity *) pool->GetSlot(y);

							if (pEntity && pEntity->GetArchetype())
							{
								const int modelIndex = pEntity->GetModelIndex();

								if (modelIndex == objIndex.Get())
								{
									Vec3V pos = pEntity->GetTransform().GetPosition();
									formatf(finalLine, "%50s|%8.2f|%8.2f|%8.2f|%-30s|%s",
										name, pos.GetXf(), pos.GetYf(), pos.GetZf(), GetEntityTypeStr(pEntity->GetType()),
										(pEntity->GetDrawHandlerPtr()) ? "" : "NO DRAW HANDLER");

									OutputLine(stream, toScreen, (pEntity->GetDrawHandlerPtr()) ? Color_yellow : Color_purple, finalLine);
								}
							}
						}
					}
				}
			}
		}
	}
}

#if __DEV
void CStreamingDebug::UpdateObjectFraming()
{
	// frame entity?
	bool focusForward = CControlMgr::GetKeyboard().GetKeyJustDown(KEY_F, KEYBOARD_MODE_DEBUG, "frame selected entity");
	bool focusBackward = CControlMgr::GetKeyboard().GetKeyJustDown(KEY_F, KEYBOARD_MODE_DEBUG_SHIFT, "frame selected entity");
	if(focusForward || focusBackward)
	{
        bool shouldFrame = true;
        Vector3 vCentre(0.0f,0.0f,0.0f);
        float radius = 0.0f;

		if (!s_entityListNum)
		{
			if ( gCurrentEntityListObj.IsValid() )
			{
				s_entityListNum = GetEntityList(gCurrentEntityListObj, &s_entityList[0], 256);
			}
		}

        if(s_entityListNum <= 0)
        {
            grcDebugDraw::PrintToScreenCoors("No entity found referencing this object", 16, 16, Color32(1.0f,0.0f,0.0f), 0, 20);
            shouldFrame = false;
        }
		
        else
        {
            static s32 focusCycle = -1;
		    focusCycle += focusForward ? 1 : -1;
		    CEntity * pEntity = s_entityList[focusCycle % s_entityListNum];
		
		    if(pEntity)
            {
				if (bSelectDontFrameEntities)
				{
					g_PickerManager.AddEntityToPickerResults(pEntity, true, true);
					shouldFrame = false;
				}
				else
				{
					if(CModelInfo::IsValidModelInfo(pEntity->GetModelIndex()))
					{
						radius = pEntity->GetBoundCentreAndRadius(vCentre);
					}
					else if(pEntity->GetCurrentPhysicsInst())
					{
						vCentre = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
						radius = pEntity->GetCurrentPhysicsInst()->GetArchetype()->GetBound()->GetRadiusAroundCentroid();
					}
					else
					{
						vCentre = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
						radius = 10.0f;
					}
				}
            }
            else
            {
                grcDebugDraw::PrintToScreenCoors("Could not get bounds to frame entity", 16, 16, Color32(1.0f,0.0f,0.0f), 0, 20);
                shouldFrame = false;
            }
        }

		if(shouldFrame)
		{
			float aspect = 1.0f;
			const CViewport* gameViewport = gVpMan.GetGameViewport();
			if(gameViewport)
			{
				const grcViewport& viewport = gameViewport->GetGrcViewport();
				aspect = (float)viewport.GetWidth() / (float)viewport.GetHeight();
			}

			camDebugDirector& debugDirector = camInterface::GetDebugDirector();
			debugDirector.ActivateFreeCam();	//Turn on debug free cam.

			//Move free camera to desired place.
			camFrame& freeCamFrame = debugDirector.GetFreeCamFrameNonConst();
			float tanFov = tanf(0.5f * DtoR * freeCamFrame.GetFov());
			tanFov = MIN(tanFov, tanFov * aspect);
			float distance = MAX(radius / tanFov, radius + freeCamFrame.GetNearClip());
			Vector3 vPosition = vCentre - distance * freeCamFrame.GetFront();
			freeCamFrame.SetPosition(vPosition);

			CControlMgr::SetDebugPadOn(true);
		}
	}
}

// Sets the resource (drawable or texture) which we want to see the thing in the world that use it
void CStreamingDebug::SetResourceForEntityFraming(strIndex streamingIndex)
{
	s_entityListNum = GetEntityList(streamingIndex, &s_entityList[0], 256);
}

#endif // __DEV

static void AppendFileInfoName(strIndex index, bool isResident, char *buffer, size_t bufferSize)
{
	if (isResident)
	{
		safecat(buffer, "(", bufferSize);
	}

	safecat(buffer, strStreamingEngine::GetObjectName(index), bufferSize);
	if (isResident)
	{
		safecat(buffer, ")", bufferSize);
	}

	safecat(buffer, " ", bufferSize);
}

void CStreamingDebug::CreateDependencyString(strIndex index, bool unfulfilledOnly, char *buffer, size_t bufferSize)
{
	strIndex deps[STREAMING_MAX_DEPENDENCIES];

	buffer[0] = 0;

	strStreamingInfo &info = strStreamingEngine::GetInfo().GetStreamingInfoRef(index);
	strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModule(index);
	s32 numDeps = pModule->GetDependencies(pModule->GetObjectIndex(index), &deps[0], STREAMING_MAX_DEPENDENCIES);

	for (s32 i = 0; i < numDeps; ++i)
	{
		strIndex depIndex = deps[i];
		strStreamingInfo &depInfo = strStreamingEngine::GetInfo().GetStreamingInfoRef(depIndex);
		bool isResident = (depInfo.GetStatus() == STRINFO_LOADED);

		if (!unfulfilledOnly || !isResident)
		{
			AppendFileInfoName(depIndex, isResident, buffer, bufferSize);
		}
	}

	// The archive itself is a dependency too.
	if (!info.IsPackfileHandle())
	{
		strLocalIndex imageIndex = strPackfileManager::GetImageFileIndexFromHandle(info.GetHandle());
		if (imageIndex != -1)
		{
			strIndex archiveStrIndex = strPackfileManager::GetStreamingModule()->GetStreamingIndex(imageIndex);
			bool isResident = strStreamingEngine::GetInfo().GetStreamingInfoRef(archiveStrIndex).GetStatus() != STRINFO_LOADED;

			if (!unfulfilledOnly || !isResident)
			{
				AppendFileInfoName(archiveStrIndex, isResident, buffer, bufferSize);
			}
		}
	}
}

#if __DEV
//
// name:		CStreamingDebug::DisplayModelsInMemory
// description:	Display all the models in memory of a certain type
template<class _T> void DisplayModelsInMemory(fwArchetypeDynamicFactory<_T>& store)
{
	atArray<_T*> ptrArray;
	store.GatherPtrs(ptrArray);

	s32 size = ptrArray.GetCount();

	char tmpName[16] = {0};
	bool isVeh = false;
	//if (store.GetEntry(0).GetModelType() == MI_TYPE_PED)
	if (ptrArray[0]->GetModelType() == MI_TYPE_PED)
	{
		formatf(tmpName, "PEDS");
	}
	//else if (store.GetEntry(0).GetModelType() == MI_TYPE_VEHICLE)
	if (ptrArray[0]->GetModelType() == MI_TYPE_VEHICLE)
	{
		formatf(tmpName, "VEHICLES");
		isVeh = true;
	}
	else
	{
		formatf(tmpName, "WEAPONS");
	}

	grcDebugDraw::AddDebugOutputEx(false, "%20s    %12s   %12s   Virtual Tex   Physical Tex   Num Refs   Flags", tmpName, isVeh ? "Virtual Frag" : "Virtual Geom", isVeh ? "Physical Frag" : "Physical Geom");
	while(size--)
	{
		fwModelId modelId;
		CModelInfo::GetBaseModelInfoFromHashKey(ptrArray[size]->GetHashKey(), &modelId);
		streamAssertf(modelId.IsValid(), "Model doesn't exist");

		if(CModelInfo::HaveAssetsLoaded(modelId))
		{
			u32 virtGeom=0, physGeom=0, virtTex=0, physTex=0, virtClip=0, physClip=0, virtFrag=0, physFrag=0, virtTotal=0, physTotal=0;
			char flags[64];

			flags[0] = '\0';
			if(CModelInfo::GetAssetStreamFlags(modelId) & STRFLAG_DONTDELETE)
				strcat(flags, "dont ");
			if(CModelInfo::GetAssetStreamFlags(modelId) & STRFLAG_MISSION_REQUIRED)
				strcat(flags, "script ");
			if(CModelInfo::GetAssetStreamFlags(modelId) & STRFLAG_INTERIOR_REQUIRED)
				strcat(flags, "interior ");
			if(CModelInfo::GetAssetStreamFlags(modelId) & STRFLAG_LOADSCENE)
				strcat(flags, "loadscene ");

			const atArray<strIndex>& ignoreList = gPopStreaming.GetResidentObjectList();
			CGtaPickerInterface::GetObjectAndDependenciesSplitSizes(NULL, modelId, ignoreList.GetElements(), ignoreList.GetCount(),
				virtGeom, physGeom, virtTex, physTex, virtClip, physClip, virtFrag, physFrag, virtTotal, physTotal);

			grcDebugDraw::AddDebugOutputEx(false, "%20s    %7d        %7d         %7d         %7d       %3d       %s",
				ptrArray[size]->GetModelName(), isVeh ? virtFrag : virtGeom, isVeh ? physFrag : physGeom, virtTex, physTex, ptrArray[size]->GetNumRefs(), flags);
		}
	}
}

//
// name:		CStreamingDebug::WriteModelSizes
// description:	Display all the models in memory of a certain type
template<class _T> void WriteModelSizes(fwArchetypeDynamicFactory<_T>& store)
{
	atArray<_T*> typeArray;
	store.GatherPtrs(typeArray);

	s32 size = typeArray.GetCount();
	while (size--)
	{
		fwModelId modelId;
		CModelInfo::GetBaseModelInfoFromHashKey(typeArray[size]->GetHashKey(), &modelId);
		streamAssertf(modelId.IsValid(), "Model doesn't exist");

		u32 virtualMem=0, physicalMem=0;
		const atArray<strIndex>& ignoreList = gPopStreaming.GetResidentObjectList();
		CModelInfo::GetObjectAndDependenciesSizes(modelId, virtualMem, physicalMem, ignoreList.GetElements(), ignoreList.GetCount());
		streamDisplayf("%s, %d, %d, %d", 
			typeArray[size]->GetModelName(),
			virtualMem>>10, 
			physicalMem>>10, 
			typeArray[size]->GetNumRefs());
	}
}

Vector2 GetWorldMouseCoords()
{
	static float s_worldX, s_worldY;
	static int s_cachedX, s_cachedY; 

	// Get the raw mouse position.
	const int  mouseScreenX = ioMouse::GetX();
	const int  mouseScreenY = ioMouse::GetY();
	// Get the world position of the mouse over the Vector Map.
	if (s_cachedX != mouseScreenX || s_cachedY != mouseScreenY)
	{
		// Convert the mouse position into VectorMap coordinates.
		// Scale mouse position so x and y are between 0 and 1.
		const float mapX = mouseScreenX / float(grcViewport::GetDefaultScreen()->GetWidth());
		const float mapY = mouseScreenY / float(grcViewport::GetDefaultScreen()->GetHeight());

		// Convert the VectorMap coordinates into World coordinates.
		CVectorMap::ConvertPointMapToWorld(mapX, mapY, s_worldX, s_worldX);
	}

	return Vector2(s_worldX, s_worldY);
}

//
// name:		CStreamingDebug::MemoryStatsObjectsInMemory
// description:	Display all the txds in memory
void CStreamingDebug::MemoryStatsObjectsInMemory(s32 moduleId, s32 &total, s32 &virt, s32 &phys, s32 &largest)
{
	total = 0;
	virt = 0;
	phys = 0;
	largest = 0;

	strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(moduleId);
	for(s32 i=0; i<pModule->GetCount(); i++)
	{
		if(pModule->HasObjectLoaded(strLocalIndex(i)) && pModule->IsObjectInImage(strLocalIndex(i)))
		{
			int virtSize, physSize;
			strIndex index = pModule->GetStreamingIndex(strLocalIndex(i));
			GetSizesSafe(index, *(strStreamingEngine::GetInfo().GetStreamingInfo(index)), virtSize, physSize, false);

			s32 totalSize = virtSize + physSize;

			total += totalSize;
			virt  += virtSize;
			phys  += physSize;
			if (totalSize>largest)
			{
				largest = totalSize;
			}
		}
	}
}
#endif //__DEV

typedef PoolTracker* PoolTrackerEntry;
s32 ComparePoolArray(const PoolTrackerEntry* ppPool1, const PoolTrackerEntry* ppPool2)
{
	if (bSortBySize)
	{
		s32 size1 = (*ppPool1)->GetActualMemoryUse();
		s32 size2 = (*ppPool2)->GetActualMemoryUse();
		return (size2 < size1) ? -1 : ((size1 == size2) ? 0 : 1);
	}
	else
	{
		const char* pszName1 = (*ppPool1)->GetName();
		const char* pszName2 = (*ppPool2)->GetName();
		return stricmp(pszName1, pszName2);
	}
};

void PrintPoolUse(const PoolTracker* tracker)
{
    streamDisplayf("%32s %5d(%5d)/%5d  %" SIZETFMT "u  %" SIZETFMT "u %d %32s", 
        tracker->GetName(), 
        tracker->GetNoOfUsedSpaces(), 
        tracker->GetPeakSlotsUsed(), 
        tracker->GetSize(), 
        tracker->GetStorageSize(),
        (tracker->GetStorageSize() * tracker->GetSize()) / 1024,
        tracker->GetActualMemoryUse()/1024,
		tracker->GetClassName());
}

void CStreamingDebug::PrintPoolUsage()
{
    streamDisplayf("Pool name                         used( peak)/total       sto(b)     size(KB)  actSize(KB)");

	atArray<PoolTracker*>& trackers = PoolTracker::GetTrackers();
    trackers.QSort(0, -1, ComparePoolArray);
    for (s32 i = 0; i < trackers.GetCount(); ++i)
        PrintPoolUse(trackers[i]);
}

typedef fwAssetStoreBase* StoreEntry;
s32 CompareStoreArray(const StoreEntry* ppStore1, const StoreEntry* ppStore2)
{
    const char* pszName1 = (*ppStore1)->GetModuleName();
    const char* pszName2 = (*ppStore2)->GetModuleName();
    return stricmp(pszName1, pszName2);
};

void CStreamingDebug::PrintStoreUsage()
{
    streamDisplayf("Store name                        used/total");

    g_storesTrackingArray.QSort(0, -1, CompareStoreArray);
    for (s32 i=0; i<g_storesTrackingArray.GetCount(); i++)
    {
        const fwAssetStoreBase* temp = g_storesTrackingArray[i];
        streamDisplayf("%32s %5d/%5d", temp->GetModuleName(), temp->GetNumUsedSlots(), temp->GetSize());
    }
}

void DisplayPoolUsage(const PoolTracker* tracker, u32* totalUsed)
{
	Color32 col(0xFFFFFFFF);
	if (tracker->GetStorageSize() < 64 && tracker->GetStorageSize() > 0)
	{
		col = Color32(255,132,72);
	}
	
	if( tracker->GetActualMemoryUse() > tracker->GetMemoryUsed() * 1.5 )
	{
		col = Color32(255,0,0);
	}

	char storageSizeStr[32];
	if(tracker->GetStorageSize() == 0)
	{
		formatf(storageSizeStr, "??");
	}
	else
	{
		formatf(storageSizeStr, "%d", tracker->GetStorageSize());
	}
	grcDebugDraw::AddDebugOutputEx(false,col,"%32s %5d(%5d)/%5d  %11s  %11d  %11d", 
									tracker->GetName(), 
									tracker->GetNoOfUsedSpaces(), 
									tracker->GetPeakSlotsUsed(),  
									tracker->GetSize(), 
									storageSizeStr,
									tracker->GetMemoryUsed() / 1024,
									tracker->GetActualMemoryUse()/1024);
	(*totalUsed) += tracker->GetActualMemoryUse();
}

void DisplayPoolUsage(const char* pszTitle, size_t used, size_t peak, size_t total)
{
	streamAssert(pszTitle);

	grcDebugDraw::AddDebugOutputEx(false, "%24s %8d %8d %8d", 
		pszTitle, 
		used, 
		peak, 
		total);
}

void DisplayMemoryInfo(const char* pszTitle, sysMemPoolAllocator* pAllocator)
{
	streamAssert(pszTitle && pAllocator);

	grcDebugDraw::AddDebugOutputEx(false, "%24s %8d %8d %8d %8d %8d %8d %8d %8d %5d%%  0x%0.8p - 0x%0.8p", 
		pszTitle, 
		(pAllocator->GetMemoryUsed() >> 10), 
		(pAllocator->GetMemoryAvailable() >> 10), 
		(pAllocator->GetHeapSize() >> 10), 
		(pAllocator->GetPeakMemoryUsed() >> 10), 
		pAllocator->GetNoOfUsedSpaces(),
		pAllocator->GetNoOfFreeSpaces(),
		pAllocator->GetPeakSlotsUsed(),
		pAllocator->GetSize(),
		pAllocator->GetFragmentation(), 
		pAllocator->GetHeapBase(), 		
		static_cast<u8*>(pAllocator->GetHeapBase()) + pAllocator->GetHeapSize());
}

void DisplayMemoryInfo(const char* pszTitle, sysMemAllocator* pAllocator)
{
	if (!pAllocator)
		return;

	grcDebugDraw::AddDebugOutputEx(false, "%24s %8d %8d %8d %8d %5d%%  0x%0.8p - 0x%0.8p", 
		pszTitle, 
		(pAllocator->GetMemoryUsed() >> 10), 
		(pAllocator->GetMemoryAvailable() >> 10), 
		(pAllocator->GetHeapSize() >> 10), 
		(pAllocator->GetHighWaterMark(false) >> 10), 
		pAllocator->GetFragmentation(), 
		pAllocator->GetHeapBase(), 		
		static_cast<u8*>(pAllocator->GetHeapBase()) + pAllocator->GetHeapSize());
}

#if MEMORY_TRACKER
void DisplayMemoryInfo(const char* pszTitle, sysMemSystemTracker* pTracker)
{
	streamAssert(pszTitle && pTracker);

	grcDebugDraw::AddDebugOutputEx(false, "%24s %8d %8d %8d", 
		pszTitle, 
		(pTracker->GetUsed() >> 10), 
		(pTracker->GetAvailable() >> 10), 
		(pTracker->GetSize() >> 10));
}
#endif

#if RESOURCE_POOLS
void DisplayMemoryInfo(const char* pszName, sysBuddyPool* pPool)
{
	if (!pPool)
		return;

	grcDebugDraw::AddDebugOutputEx(false, "%24s %8d %8d %8" SIZETFMT "u %8" SIZETFMT "u %8" SIZETFMT "u", 
		pszName, 
		pPool->GetUsedSize(), 
		pPool->GetPeakSize(), 
		pPool->GetSize(), 
		pPool->GetStorageSize(),
		(pPool->GetSize() * pPool->GetStorageSize()) / 1024);
}
#endif

void DisplayMemoryInfo(const char* pszName, atPoolBase* pPool)
{
	streamAssert(pPool);

	grcDebugDraw::AddDebugOutputEx(false, "%24s %8d %8d %8" SIZETFMT "u %8" SIZETFMT "u %8" SIZETFMT "u", 
		pszName, 
		pPool->GetNoOfUsedSpaces(), 
		pPool->GetPeakSlotsUsed(), 
		pPool->GetSize(), 
		pPool->GetStorageSize(), 		
		(pPool->GetStorageSize() * static_cast<size_t>(pPool->GetSize())) / 1024);
}

template <typename T>
void DisplayMemoryInfo(fwPool<T>* pPool)
{
	streamAssert(pPool);
	
	grcDebugDraw::AddDebugOutputEx(false, "%24s %8d %8d %8" SIZETFMT "u  %8" SIZETFMT "u", 
		pPool->GetName(), 
		pPool->GetNoOfUsedSpaces(), 
		pPool->GetPeakSlotsUsed(), 
		pPool->GetSize(), 
		pPool->GetStorageSize(), 		
		(pPool->GetStorageSize() * static_cast<size_t>(pPool->GetSize())) / 1024);
}

template <typename T, bool _Iterable, typename _Allocator>
void DisplayMemoryInfo(const char* pszName, atPagedPool<T, _Iterable, _Allocator>* pPool)
{
	streamAssert(pPool);

	grcDebugDraw::AddDebugOutputEx(false, "%24s %8d %8d %8" SIZETFMT "u %8" SIZETFMT "u %8" SIZETFMT "u", 
		pszName, 
		pPool->GetCount(), 
		pPool->GetPeakElementUsageForSubType(0),
		pPool->GetMaxPageCount() * pPool->GetMaxElementsPerPage(), 
		sizeof(T), 		
		pPool->GetMemoryUsage(0));
}

void DisplayMemoryInfo(const char* pszTitle, void* const ptr, s32 usedBytes, size_t freeBytes, size_t totalBytes, size_t highBytes, size_t fragPercent)
{
	streamAssert(pszTitle && ptr);

	grcDebugDraw::AddDebugOutputEx(false, "%24s %8d %8d %8d %8d %5d%%  0x%0.8p - 0x%0.8p", 
		pszTitle, 
		(usedBytes >> 10), 
		(freeBytes >> 10), 
		(totalBytes >> 10), 
		(highBytes >> 10),
		fragPercent,
		ptr, 
		static_cast<u8*>(ptr) + totalBytes);
}

void DisplayMemoryInfo(const char* pszTitle, size_t usedBytes, size_t freeBytes, size_t totalBytes)
{
	streamAssert(pszTitle);

	grcDebugDraw::AddDebugOutputEx(false, "%24s %8d %8d %8d", 
		pszTitle, 
		(usedBytes >> 10), 
		(freeBytes >> 10), 
		(totalBytes >> 10));
}

void DisplayMemoryInfo(const char* pszTitle, size_t usedBytes)
{
	streamAssert(pszTitle);

	grcDebugDraw::AddDebugOutputEx(false, "%24s %8d", 
		pszTitle, 
		(usedBytes >> 10));
}


void MemoryStressTestFunc(void *ptr)
{
	MemoryStressTester *tester = reinterpret_cast<MemoryStressTester *>(ptr);

	int heapIndex = tester->m_Heap;
	u32 minAlloc = tester->m_MinAlloc;
	u32 maxAlloc = tester->m_MaxAlloc;
	int minWait = tester->m_MinWait;
	int maxWait = tester->m_MaxWait;
	bool isRsc = (heapIndex == MEMTYPE_RESOURCE_VIRTUAL || heapIndex == MEMTYPE_RESOURCE_PHYSICAL);

	while (!s_DeleteAllStressTesters)
	{
		// Allocate or delete?
		if (tester->m_Allocs.GetCount() == 0 || (tester->m_Allocs.GetCount() < tester->m_MaxAllocCount && (rand() & 1)))
		{
			// Allocate.
			void *ptr;
			u32 size = (u32) rand();

			if (maxAlloc > minAlloc)
			{
				size %= maxAlloc - minAlloc;
				size += minAlloc;
			}
			else
			{
				size = minAlloc;
			}

			sysMemAllowResourceAlloc++;
#if __XENON
			if (heapIndex == MEMTYPE_COUNT)
			{
				ptr = XMemAlloc(size, MAKE_XALLOC_ATTRIBUTES(0, FALSE, TRUE, FALSE, eXALLOCAllocatorId_XGRAPHICS, XALLOC_ALIGNMENT_16, XALLOC_MEMPROTECT_WRITECOMBINE, FALSE, XALLOC_MEMTYPE_HEAP));
			}
			else
#endif // __XENON
			{
				if (isRsc)
				{
					MEM_USE_USERDATA(MEMUSERDATA_DEBUGSTEAL);
					ptr = strStreamingEngine::GetAllocator().Allocate(size, 16, heapIndex);
				}
				else
				{
					ptr = sysMemAllocator::GetMaster().RAGE_LOG_ALLOCATE_HEAP(size, 16, heapIndex);
				}
			}

			tester->m_Allocs.Append() = ptr;

			sysMemAllowResourceAlloc--;
		}
		else
		{
			// Time to free something.
			int index = rand() % tester->m_Allocs.GetCount();

			void *ptr = tester->m_Allocs[index];

#if __XENON
			if (heapIndex == MEMTYPE_COUNT)
			{
				XMemFree(ptr, MAKE_XALLOC_ATTRIBUTES(0, FALSE, TRUE, FALSE, eXALLOCAllocatorId_XGRAPHICS, XALLOC_ALIGNMENT_16, XALLOC_MEMPROTECT_WRITECOMBINE, FALSE, XALLOC_MEMTYPE_HEAP));
			}
			else
#endif //  __XENON
			{
				if (isRsc)
				{
					MEM_USE_USERDATA(MEMUSERDATA_DEBUGSTEAL);
					strStreamingEngine::GetAllocator().Free(ptr);
				}
				else
				{
					sysMemAllocator::GetMaster().Free(ptr);
				}
			}

			tester->m_Allocs.DeleteFast(index);
		}

		unsigned delay = (unsigned) fwRandom::GetRandomNumberInRange(minWait, maxWait);
		sysIpcSleep(delay);
	}
}

void DisplayStressTestInfo()
{
	// This code isn't really thread-safe, and doesn't care.
	// It makes sure that it doesn't do any at*Array calls that could trigger an out-of-bounds error,
	// other than that, it may get some pointers wrong. And it doesn't care.

	int testers = s_MemoryStressTesters.GetCount();

	for (int x=0; x<testers; x++)
	{
		MemoryStressTester &tester = s_MemoryStressTesters[x];

		void **allocs = tester.m_Allocs.GetElements();
		int allocCount = tester.m_Allocs.GetCount();			// Modified by a separate thread. We don't care! We might be reading outside the "count" bound, but that's cool.

		char tempLine[256];
		tempLine[0] = 0;

		for (int y=0; y<allocCount; y++)
		{
			char temp[256];
			formatf(temp, "%p ", allocs[y]);
			safecat(tempLine, temp);
		}

		grcDebugDraw::AddDebugOutputEx(false, "%d: (%d) %s", x, allocCount, tempLine);
	}
}

void CreateMemoryStressTestThread()
{
	if (Verifyf(s_MemoryStressTesters.GetCount() < s_MemoryStressTesters.GetCapacity(), "Too many stress testers"))
	{
		MemoryStressTester &tester = s_MemoryStressTesters.Append();
		tester = s_MemoryStressTestSetup;
		tester.m_ThreadId = sysIpcCreateThread(MemoryStressTestFunc, &tester, sysIpcMinThreadStackSize, (sysIpcPriority) tester.m_Prio, "Memory Stress Test", tester.m_CpuId);
	}
}

//
// name:		DisplayMemoryUsage
// description:	Display how much virtual, physical memory is used
void DisplayMemoryUsage()
{
#if RSG_ORBIS || RSG_DURANGO
	grcDebugDraw::AddDebugOutputEx(false, "");
#endif

#if ENABLE_DEFRAG_CALLBACK && ENABLE_DEFRAG_DEBUG
	grcDebugDraw::AddDebugOutputEx(false, "Defrag Total: %d", strDefragmentation::s_defragRemoveTotal); 
	grcDebugDraw::AddDebugOutputEx(false, "Defrag Current: %d", strDefragmentation::s_defragRemoveCurrent);
	grcDebugDraw::AddDebugOutputEx(false, "Defrag Peak: %d", strDefragmentation::s_defragRemovePeak);
	grcDebugDraw::AddDebugOutputEx(false, "Defrag Average: %f", (float) strDefragmentation::s_defragRemoveTotal / (float) fwTimer::GetFrameCount());
	grcDebugDraw::AddDebugOutputEx(false, "Frame Count: %d", fwTimer::GetFrameCount());
#endif

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// MAIN
	//
	grcDebugDraw::AddDebugOutputEx(false, "%24s %8s %8s %8s %8s %6s  %s", "CPU ALLOCATOR", "USED KB", "FREE KB", "TOTAL KB", "HIGH KB", "FRAG %", "ADDRESS RANGE");

	// Game Virtual
	sysMemAllocator* pAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL);
	DisplayMemoryInfo("Game Virtual", pAllocator);

	size_t usedBytes = 0;
	sysMemSimpleAllocator* pSimpleAllocator = dynamic_cast<sysMemSimpleAllocator*>(pAllocator);
	if (pSimpleAllocator)
	{
		usedBytes = pSimpleAllocator->GetSmallocatorTotalSize();
		DisplayMemoryInfo("Smallocator", usedBytes);
	}	

	// Debug Virtual
#if ENABLE_DEBUG_HEAP
	pAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_DEBUG_VIRTUAL);
	DisplayMemoryInfo("Debug Virtual", pAllocator);
#endif

	// Hemlis Virtual
#if !__FINAL && (RSG_ORBIS || RSG_DURANGO)
	DisplayMemoryInfo("Hemlis Virtual", MEMMANAGER.GetHemlisAllocator());
	DisplayMemoryInfo("Recorder", MEMMANAGER.GetRecorderAllocator());
#endif

	// Header Virtual
#if RESOURCE_HEADER
	pAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_HEADER_VIRTUAL);
	DisplayMemoryInfo("Header Virtual", pAllocator);

	DisplayMemoryInfo("Header Virtual Pool", sysMemManager::GetInstance().GetUsedHeaders() << 10, (sysMemManager::GetInstance().GetTotalHeaders() - sysMemManager::GetInstance().GetUsedHeaders()) << 10, sysMemManager::GetInstance().GetTotalHeaders() << 10);
#endif

	// Resource Virtual
	pAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_VIRTUAL);
	DisplayMemoryInfo("Resource Virtual", pAllocator);

	// Streaming Virtual
#if ONE_STREAMING_HEAP
	usedBytes = (size_t) strStreamingEngine::GetAllocator().GetPhysicalMemoryUsed();
	size_t totalBytes = (size_t) strStreamingEngine::GetAllocator().GetPhysicalMemoryAvailable();
#else // !ONE_STREAMING_HEAP
#if !RSG_PC
	usedBytes = (size_t) strStreamingEngine::GetAllocator().GetVirtualMemoryUsed();
	size_t totalBytes = (size_t) strStreamingEngine::GetAllocator().GetVirtualMemoryAvailable();
#else // RSG_PC
	usedBytes  = sysMemAllocator::GetCurrent().GetAllocator(MEMTYPE_RESOURCE_VIRTUAL)->GetMemoryUsed(-1);
	size_t totalBytes = usedBytes + sysMemAllocator::GetCurrent().GetAllocator(MEMTYPE_RESOURCE_VIRTUAL)->GetMemoryAvailable();
#endif // !RSG_PC
#endif // ONE_STREAMING_HEAP

	size_t freeBytes = totalBytes - usedBytes;
	DisplayMemoryInfo("Streaming Virtual", usedBytes, freeBytes, totalBytes);

	// Streaming Physcial
#if !ONE_STREAMING_HEAP
#if !RSG_PC
	usedBytes = (size_t) strStreamingEngine::GetAllocator().GetPhysicalMemoryUsed();
	totalBytes = (size_t) strStreamingEngine::GetAllocator().GetPhysicalMemoryAvailable();
#else // RSG_PC
	usedBytes  = sysMemAllocator::GetCurrent().GetAllocator(MEMTYPE_RESOURCE_PHYSICAL)->GetMemoryUsed(-1);
	totalBytes = usedBytes + sysMemAllocator::GetCurrent().GetAllocator(MEMTYPE_RESOURCE_PHYSICAL)->GetMemoryAvailable();
#endif // !RSG_PC

	freeBytes = totalBytes - usedBytes;
	DisplayMemoryInfo("Streaming Physical", usedBytes, freeBytes, totalBytes);
#endif // !ONE_STREAMING_HEAP

	// External Virtual
#if ONE_STREAMING_HEAP
	usedBytes = (s32) strStreamingEngine::GetAllocator().GetExternalPhysicalMemoryUsed();	
#else
	usedBytes = (s32) strStreamingEngine::GetAllocator().GetExternalVirtualMemoryUsed();
#endif

#if USE_RESOURCE_CACHE
	DisplayMemoryInfo("Streaming Video", grcResourceCache::GetInstance().GetTotalUsedMemory(MT_Video), grcResourceCache::GetInstance().GetStreamingMemory(), grcResourceCache::GetInstance().GetTotalMemory(MT_Video));
#endif // USE_RESOURCE_CACHE

	DisplayMemoryInfo("External Virtual", usedBytes);

#if !RSG_PC
	// Slosh Virtual
	DisplayMemoryInfo("Slosh Virtual", (s32) pAllocator->GetMemoryAvailable() - freeBytes);
#endif

	// Frag Cache
	DisplayMemoryInfo("Frag Cache", FRAGCACHEALLOCATOR);

#if RSG_ORBIS
	// Flex 
	DisplayMemoryInfo("Flex", MEMMANAGER.GetFlexAllocator());
#endif

#if RSG_ORBIS || RSG_DURANGO || RSG_PC
	// Replay 
	DisplayMemoryInfo("Replay", MEMMANAGER.GetReplayAllocator());
#endif

#if __XENON
	// Pool Virtual
	if (g_pXenonPoolAllocator)
		DisplayMemoryInfo("Pool Virtual", g_pXenonPoolAllocator);
#endif

#if SCRATCH_ALLOCATOR
	sysMemStack* pStack = sysMemManager::GetInstance().GetScratchAllocator();
	DisplayMemoryInfo("Physics Scratch", pStack);
#endif

	// System
#if MEMORY_TRACKER
	DisplayMemoryInfo("System", sysMemManager::GetInstance().GetSystemTracker());
#elif RSG_ORBIS
	size_t used = 0;
	int32_t result = 0;
	void* pAddress = NULL;
	SceKernelVirtualQueryInfo info;
	
	while ((result = sceKernelVirtualQuery(pAddress, SCE_KERNEL_VQ_FIND_NEXT, &info, sizeof(SceKernelVirtualQueryInfo))) != SCE_KERNEL_ERROR_EACCES)
	{
		pAddress = info.end;
		const size_t size = reinterpret_cast<size_t>(info.end) - reinterpret_cast<size_t>(info.start);				
		used += size;
	}

	DisplayMemoryInfo("System", used, sceKernelGetDirectMemorySize() - used, sceKernelGetDirectMemorySize());
#elif RSG_DURANGO
	TITLEMEMORYSTATUS status;
	status.dwLength =  sizeof(TITLEMEMORYSTATUS);
	TitleMemoryStatus(&status);
	DisplayMemoryInfo("System", status.ullTotalMem - status.ullAvailMem, status.ullAvailMem, status.ullTotalMem);
#endif

#if COMMERCE_CONTAINER
	// Residual
	if (g_pResidualAllocator)
		DisplayMemoryInfo("Residual", g_pResidualAllocator);

	// Flex
	DisplayMemoryInfo("Flex", sysMemManager::GetInstance().GetFlexAllocator()->GetPrimaryAllocator());
	DisplayMemoryInfo("Move Ped", sysMemManager::GetInstance().GetMovePedMemory(), MOVEPED_HEAP_SIZE, 0, MOVEPED_HEAP_SIZE, MOVEPED_HEAP_SIZE, 0);
#endif

#if COMMERCE_CONTAINER
	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// VRAM
	//
	grcDebugDraw::AddDebugOutputEx(false, "");
	grcDebugDraw::AddDebugOutputEx(false, "%24s %8s %8s %8s %8s %6s  %s", "GPU ALLOCATOR", "USED KB", "FREE KB", "TOTAL KB", "HIGH KB", "FRAG %", "ADDRESS RANGE");

	// Game Physical
	pAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_PHYSICAL);
	DisplayMemoryInfo("Game Physical", pAllocator);

	// Resource Physical
	pAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_PHYSICAL);
	DisplayMemoryInfo("Resource Physical", pAllocator);

	// Streaming Physical
	usedBytes = strStreamingEngine::GetAllocator().GetPhysicalMemoryUsed();
	totalBytes = strStreamingEngine::GetAllocator().GetPhysicalMemoryAvailable();
	freeBytes = totalBytes - usedBytes;

	DisplayMemoryInfo("Streaming Physical", usedBytes, freeBytes, totalBytes);

	// External Physical
	usedBytes = (s32) strStreamingEngine::GetAllocator().GetExternalPhysicalMemoryUsed();
	DisplayMemoryInfo("External Physical", usedBytes);

	// Slosh Physical
	DisplayMemoryInfo("Slosh Physical", pAllocator->GetMemoryAvailable() - freeBytes);
#endif

#if RSG_ORBIS
	usedBytes = getVideoPrivateMemory().GetMemoryUsed();
	freeBytes = getVideoPrivateMemory().GetMemoryAvailable();
	totalBytes = usedBytes + freeBytes;
	DisplayMemoryInfo("Video Memory", usedBytes, freeBytes, totalBytes);
#endif // RSG_ORBIS

#if __CONSOLE
	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// POOL ALLOCATOR
	//
	grcDebugDraw::AddDebugOutputEx(false, "");
	grcDebugDraw::AddDebugOutputEx(false, "%24s %8s %8s %8s %8s %8s %8s %8s %8s %6s  %s", "POOL ALLOCATOR KB", "USED KB", "FREE KB", "TOTAL KB", "HIGH KB", "USED", "FREE", "PEAK", "TOTAL", "FRAG %", "ADDRESS RANGE");

	DisplayMemoryInfo("audVehicleAudioEntity", audVehicleAudioEntity::GetAllocator());
#if __XENON || __PS3
	DisplayMemoryInfo("CObject", CObject::GetAllocator());
#endif
	DisplayMemoryInfo("CTask", CTask::GetAllocator());
	DisplayMemoryInfo("CTaskInfo", CTaskInfo::GetAllocator());
	DisplayMemoryInfo("CVehicle", CVehicle::GetAllocator());
	DisplayMemoryInfo("g_TxdStore", NULL, (size_t) g_TxdStore.GetNumUsedSlots() << 10, (size_t) g_TxdStore.GetNoOfFreeSpaces() << 10, (size_t) g_TxdStore.GetSize() << 10, (size_t) g_TxdStore.GetPeakSlotsUsed() << 10, 0);
#endif

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// NETWORK
	//
	grcDebugDraw::AddDebugOutputEx(false, "");
	grcDebugDraw::AddDebugOutputEx(false, "%24s %8s %8s %8s %8s %6s  %s", "NETWORK KB", "USED KB", "FREE KB", "TOTAL KB", "HIGH KB", "FRAG %", "ADDRESS RANGE");

	// Network
	pAllocator = CNetwork::GetNetworkHeap();
	DisplayMemoryInfo("Network", pAllocator);

	pAllocator = g_rlAllocator;
	DisplayMemoryInfo("RLine", pAllocator);

	pAllocator = &CNetwork::GetSCxnMgrAllocator();
	DisplayMemoryInfo("SCxnMgr", pAllocator);

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// POOLS
	//
	grcDebugDraw::AddDebugOutputEx(false, "");
	grcDebugDraw::AddDebugOutputEx(false, "%24s %8s %8s %8s %8s %8s", "POOL", "USED", "PEAK", "TOTAL", "SIZE", "TOTAL KB");
	
#if RESOURCE_POOLS
	PS3_ONLY(const char* pszTitle[] = {"Resource Virtual 4K Pool", "Resource Virtual 8K Pool"};)
	XENON_ONLY(const char* pszTitle[] = {"Resource Virtual 8K Pool", "Resource Virtual 16K Pool"};)

	sysMemBuddyAllocator* pBuddyAllocator = (sysMemBuddyAllocator*) sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_VIRTUAL);
	DisplayMemoryInfo(pszTitle[0], pBuddyAllocator->GetPool(0));
	DisplayMemoryInfo(pszTitle[1], pBuddyAllocator->GetPool(1));
#endif // __PS3 || __XENON

	DisplayMemoryInfo("Frag Cache Pool", fragManager::GetFragCacheAllocator()->GetPool());
	DisplayMemoryInfo("Archetype Pool", &fwArchetypeManager::GetPool());
	DisplayMemoryInfo(CLightExtension::GetPool());
	DisplayMemoryInfo("CLightEntity Pool", CLightEntity::GetPool());
	DisplayMemoryInfo("StaticBounds DefData", g_StaticBoundsStore.GetDefDataSize() << 10);
	DisplayMemoryInfo("DynamicArchetypeComponent Pool", fwDynamicArchetypeComponent::GetPool());
	
#if API_HOOKER
	DisplayMemoryInfo("g_virtualAllocTotal", MEMMANAGER.m_virtualAllocTotal);
	DisplayMemoryInfo("g_heapAllocTotal", MEMMANAGER.m_heapAllocTotal);
#endif
	//DisplayMemoryInfo("Quaternion Transform", fwQuaternionTransform::GetPool());

	//grcDebugDraw::AddDebugOutputEx(false, "");
	//grcDebugDraw::AddDebugOutputEx(false, "%24s %8s %8s %8s", "Prime", "OVERAGE", "WASTED", "TOTAL");
	//DisplayMemoryInfo("atMap", g_primeOverhead << 10, g_primeWastedOverhead << 10, (g_primeOverhead + g_primeWastedOverhead) << 10);
}

void DisplayBucketUsage(const char* pName, s32 bucket)
{
	sysMemSimpleAllocator* pGameVirtual = static_cast<sysMemSimpleAllocator*>(sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL));
	Assert(pGameVirtual);

#if __PPU
	sysMemSimpleAllocator* pFlexAllocator = static_cast<sysMemSimpleAllocator*>(sysMemManager::GetInstance().GetFlexAllocator()->GetPrimaryAllocator());
	Assert(pFlexAllocator);

	sysMemSimpleAllocator* pResidualAllocator = static_cast<sysMemSimpleAllocator*>(g_pResidualAllocator);
	Assert(pResidualAllocator);

	const size_t items = 3;
	sysMemSimpleAllocator* pAllocator[items] = {pGameVirtual, pFlexAllocator, pResidualAllocator};
#elif __XENON && ENABLE_DEBUG_HEAP
	sysMemSimpleAllocator* pPoolAllocator = static_cast<sysMemSimpleAllocator*>(g_pXenonPoolAllocator);
	Assert(pPoolAllocator);

	const size_t items = 2;
	sysMemSimpleAllocator* pAllocator[items] = {pGameVirtual, pPoolAllocator};
#else
	const size_t items = 1;
	sysMemSimpleAllocator* pAllocator[items] = {pGameVirtual};
#endif

	size_t virtualMem = 0;
	size_t virtualLow = 0;

	for (size_t i = 0; i < items; ++i)
	{
		virtualMem += pAllocator[i]->GetMemoryUsed(bucket);
		virtualLow += pAllocator[i]->GetMemorySnapshot(bucket);
	}

	size_t virtualDiff = (virtualMem - virtualLow) >> 10;

#if !ONE_STREAMING_HEAP
	grcDebugDraw::AddDebugOutputEx(false, bucket&0x1?Color_white:Color_BlueViolet, "%16s %8dK/%8dK=%8dK | %8dK | %8dK | %8dK | %8dK", pName,
		virtualMem >> 10,
		virtualLow >> 10,
		virtualDiff,
		(s32)sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_VIRTUAL)->GetMemoryUsed(bucket)/1024,
		(s32)sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_PHYSICAL)->GetMemoryUsed(bucket)/1024,
		(s32)sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_PHYSICAL)->GetMemoryUsed(bucket)/1024,
#if __PPU 
		((g_pResidualAllocator?(s32)g_pResidualAllocator->GetMemoryUsed(bucket)/1024:0)) 
#else		
		0
#endif // __PPU
		);
#else
	grcDebugDraw::AddDebugOutputEx(false, bucket&0x1?Color_white:Color_BlueViolet, "%16s %8dK/%8dK=%8dK | %8dK", pName,
		virtualMem >> 10,
		virtualLow >> 10,
		virtualDiff,
		(s32)sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_VIRTUAL)->GetMemoryUsed(bucket)/1024
		);
#endif
}

void SaveMemoryProfileLocations()
{
	if (!PARSER.SaveObject(MEMORY_LOCATION_FILE, "xml", &s_MemoryProfileLocations))
	{
		streamErrorf("Error occured when trying to save updated memory profile locations at " MEMORY_LOCATION_FILE);
	}
	else
	{
		streamDisplayf("Saved memory locations at " MEMORY_LOCATION_FILE);
	}
}

void CompareAgainstSelected()
{
	if (s_MemoryProfileLocationTarget == -1)
	{
		streamErrorf("No location selected");
	}
	else
	{
		const MemoryProfileLocation &location = s_MemoryProfileLocations.m_Locations[s_MemoryProfileLocationTarget];
		const MemoryFootprint &footprint = location.m_MemoryFootprint;
		int moduleCount = footprint.m_ModuleMemoryStatsMap.GetCount();

		if (moduleCount == 0)
		{
			streamErrorf("No memory stats have been recorded for %s", location.m_Name.c_str());
		}
		else
		{
			// Update the "expected" sizes.
			strStreamingModuleMgr& moduleMgr = strStreamingEngine::GetInfo().GetModuleMgr();
			s32 numModules = moduleMgr.GetNumberOfModules();
			int minModuleCount = Min(numModules, moduleCount);

			streamAssertf(moduleCount == numModules, "Module count mismatch - was a module removed or added? The memory profiles for the locations should be recaptured.");

			for (int x=0; x<minModuleCount; x++)
			{
				strStreamingModule *module = moduleMgr.GetModule(x);

				const MemoryProfileModuleStat *stat = footprint.m_ModuleMemoryStatsMap.SafeGet(atHashString(module->GetModuleName()));

				if (stat)
				{
					module->SetExpectedVirtualSize(stat->m_VirtualMemory);
					module->SetExpectedPhysicalSize(stat->m_PhysicalMemory);
				}
			}

			s_MemoryProfileCompareLocation = s_MemoryProfileLocationTarget;

			// Also enable the relevant display.
			bDisplayModuleMemory = true;
			bDisplayModuleMemoryDetailed = true;
		}
	}
}


void CStreamingDebug::CollectMemoryProfileInformation(int locationIndex)
{
	if ((u32) locationIndex < (u32) s_MemoryProfileLocations.m_Locations.GetCount())
	{
		streamAssertf(!s_MemoryProfileLocations.m_Locations[locationIndex].IsMetaLocation(), "%s is not a real location - you can't collect stats there.",
			s_MemoryProfileLocations.m_Locations[locationIndex].m_Name.c_str());

		strStreamingModuleMgr& moduleMgr = strStreamingEngine::GetInfo().GetModuleMgr();
		s32 numModules = moduleMgr.GetNumberOfModules();
		s32 moduleId;

		// Get the input data
		ModuleMemoryStats memStats;
		INIT_MODULE_MEMORY_STATS(&memStats, numModules);

		ComputeModuleMemoryStats(&memStats, false);

		// Prepare the output data
		MemoryFootprint &footprint = s_MemoryProfileLocations.m_Locations[locationIndex].m_MemoryFootprint;
		atBinaryMap<MemoryProfileModuleStat, atHashString> &outStats = footprint.m_ModuleMemoryStatsMap;

		outStats.Reset();
		outStats.Reserve(numModules);
/*		
		s32 totalVirtualMemSize = 0;
		s32 totalPhysicalMemSize = 0;
		s32 totalRequiredVirtualMemSize = 0;
		s32 totalRequiredPhysicalMemSize = 0;
		s32 totalRequestedVirtualMemSize = 0;
		s32 totalRequestedPhysicalMemSize = 0;
*/
		for(moduleId = 0; moduleId < numModules; moduleId++)
		{
/*			strStreamingModule* pModule = moduleMgr.GetModule(moduleId);

			totalVirtualMemSize += memStats.moduleVirtualMemSize[moduleId];
			totalPhysicalMemSize += memStats.modulePhysicalMemSize[moduleId];
			totalRequiredVirtualMemSize += memStats.moduleRequiredVirtualMemSize[moduleId];
			totalRequiredPhysicalMemSize += memStats.moduleRequiredPhysicalMemSize[moduleId];
			totalRequestedVirtualMemSize += memStats.moduleRequestedVirtualMemSize[moduleId];
			totalRequestedPhysicalMemSize += memStats.moduleRequestedPhysicalMemSize[moduleId];
*/
			s32 sceneVirtual = (memStats.moduleRequiredVirtualMemSize[moduleId] + memStats.moduleRequestedVirtualMemSize[moduleId]) >> 10;
			s32 scenePhysical = (memStats.moduleRequiredPhysicalMemSize[moduleId] + memStats.moduleRequestedPhysicalMemSize[moduleId])>> 10;

			MemoryProfileModuleStat *stat = outStats.Insert(atHashString(moduleMgr.GetModule(moduleId)->GetModuleName()));
			stat->m_VirtualMemory = sceneVirtual;
			stat->m_PhysicalMemory = scenePhysical;
		}

		outStats.FinishInsertion();
	}
}

void CStreamingDebug::CollectMemoryProfileInformationFromCurrent()
{
	CollectMemoryProfileInformation(s_MemoryProfileLocationTarget);
}

void CStreamingDebug::ShowCollectedMemoryProfileStats(bool toScreen)
{
	strStreamingModuleMgr& moduleMgr = strStreamingEngine::GetInfo().GetModuleMgr();
	s32 numModules = moduleMgr.GetNumberOfModules();
	bool *moduleHasData = Alloca(bool, numModules);
	int count = s_MemoryProfileLocations.m_Locations.GetCount();

	// Let's see which modules are worth reporting.
	memset(moduleHasData, 0, sizeof(bool) * numModules);
	for (int x=0; x<numModules; x++)
	{
		for (int y=0; y<count; y++)
		{
			const MemoryFootprint &footprint = s_MemoryProfileLocations.m_Locations[y].m_MemoryFootprint;
			const MemoryProfileModuleStat *stat = footprint.m_ModuleMemoryStatsMap.SafeGet(moduleMgr.GetModule(x)->GetModuleName());

			if (stat)
			{
				// If there's any memory used at all, report this.
				if (stat->m_VirtualMemory || stat->m_PhysicalMemory)
				{
					moduleHasData[x] = true;
					break;
				}
			}
		}
	}

	// Let's first create the header.
	// It's easier to compare numbers vertically, and we'll probably compare locations to each other. As such,
	// modules should be the columns.
	char line[256];
	char substring[128];

	formatf(line, "%20s ", "Location");

	for (int x=0; x<numModules; x++)
	{
		if (moduleHasData[x])
		{
			formatf(substring, "%15s ", moduleMgr.GetModule(x)->GetModuleName());
			safecat(line, substring);
		}
	}

	OutputLine(NULL, toScreen, Color_yellow, line);

	bool color = true;

	// Now each subsection.
	for (int x=0; x<count; x++)
	{
		const MemoryFootprint &footprint = s_MemoryProfileLocations.m_Locations[x].m_MemoryFootprint;
		formatf(line, "%20s|", s_MemoryProfileLocations.m_Locations[x].m_Name.c_str());

		for (int y=0; y<numModules; y++)
		{
			if (moduleHasData[y])
			{
				const MemoryProfileModuleStat *stat = footprint.m_ModuleMemoryStatsMap.SafeGet(moduleMgr.GetModule(y)->GetModuleName());
				formatf(substring, "%6dK|%6dK|", stat->m_VirtualMemory, stat->m_PhysicalMemory);
				safecat(line, substring);
			}
		}

		OutputLine(NULL, toScreen, (color?Color_white:Color_BlueViolet), line);
		color = !color;
	}
}

void CStreamingDebug::DumpCollectedMemoryProfileStats()
{
	ShowCollectedMemoryProfileStats(false);
}

void TeleportToMemoryProfileLocation(int debugLocationIndex)
{
	if ((u32) debugLocationIndex < (u32) s_MemoryProfileLocations.m_Locations.GetCount())
	{
		if (s_MemoryProfileLocations.m_Locations[debugLocationIndex].IsMetaLocation())
		{
			streamAssertf(false, "%s is not a real location - you can't teleport there.", s_MemoryProfileLocations.m_Locations[debugLocationIndex].m_Name.c_str());
			return;
		}

		MemoryProfileLocation &location = s_MemoryProfileLocations.m_Locations[debugLocationIndex];
		Vec3V position = location.m_PlayerPos;

		if (FindPlayerVehicle(CGameWorld::GetMainPlayerInfo(), true))
		{
			FindPlayerVehicle(CGameWorld::GetMainPlayerInfo(), true)->Teleport(RCC_VECTOR3(position), 0.0f, false);
		}
		else if(streamVerify(FindPlayerPed()))
		{
			FindPlayerPed()->Teleport(RCC_VECTOR3(position), DtoR * 0.0f);
		}

		// Set the camera
		camDebugDirector& debugDirector = camInterface::GetDebugDirector();
		if ( debugDirector.IsFreeCamActive() == false )
			debugDirector.ActivateFreeCam();

		//Move free camera to desired place.
		camFrame& freeCamFrame = debugDirector.GetFreeCamFrameNonConst();

		freeCamFrame.SetWorldMatrix(MAT34V_TO_MATRIX34(location.m_CameraMtx), false);

		g_SceneToGBufferPhaseNew->GetPortalVisTracker()->ScanUntilProbeTrue();
		g_SceneToGBufferPhaseNew->GetPortalVisTracker()->Update( VEC3V_TO_VECTOR3(location.m_CameraMtx.GetCol3()) );

		g_SceneStreamerMgr.LoadScene(RCC_VECTOR3(position));
	}
}

void TeleportToMemoryProfileLocation()
{
	TeleportToMemoryProfileLocation(s_MemoryProfileLocationTarget);
}

void ResetAutomaticDebugLocationCycle()
{
	s_AutomaticDebugCycleTimer.Reset();
	s_NextAutomaticDebugLocation = 0;
}

//
// name:		DisplayMemoryBucketUsage
// description:	Display how much virtual, physical memory is used by each memory bucket
void DisplayMemoryBucketUsage()
{
#if __PPU
	grcDebugDraw::AddDebugOutputEx(false, " Name                            Main/Low=Diff |   ResVirt |      Vram |   ResVram |      Res");
#else
	grcDebugDraw::AddDebugOutputEx(false, " Name                            Main/Low=Diff |       Res ");
#endif

	for (int i =0; i < 16; ++i)
	{
		DisplayBucketUsage(sysMemGetBucketName(MemoryBucketIds(i)), i);
	}
}

static char s_achFilterPoolName[RAGE_MAX_PATH];

//
// name:		DisplayPoolsUsage
// description:	Display how much of each pool is used, through all pools
void DisplayPoolsUsage()
{
	grcDebugDraw::AddDebugOutputEx(false,"name                              used( peak)/total       sto(b)     size(KB)  actSize(KB)");

	atArray<PoolTracker*>& trackers = PoolTracker::GetTrackers();
	trackers.QSort(0, -1, ComparePoolArray);

	atString searchString = atString(s_achFilterPoolName);
	searchString.Uppercase();
	u32 totalUsed = 0;

	for (s32 i=0; i<trackers.GetCount(); i++)
	{
		bool bDisplayPoolUsage = true;

		if (trackers[i] && searchString.GetLength()>0)
		{
			atString poolName = atString( trackers[i]->GetName() );
			poolName.Uppercase();

			bDisplayPoolUsage = ( poolName.IndexOf(searchString)!=-1 );
		}

		if (bDisplayPoolUsage)
		{
			DisplayPoolUsage(trackers[i], &totalUsed);
		}
	}

	Color32 col(0xFFFFFFFF);
	grcDebugDraw::AddDebugOutputEx(false,col,"%73s Total mem: %d", 
		" ", 
		totalUsed/1024);
	grcDebugDraw::AddDebugOutputEx(false,col,"");

#if DEBUG_MEASURE_COLLISION_RECORD_HIGH_WATER_MARK
	grcDebugDraw::AddDebugOutputEx(false,Color32(0xFFFFFFFF),"%32s %5d(%5d)/%5d  %11d  %11d  %11d", 
									"CCollisionRecord", 
									CCollisionHistory::ms_nHighWaterMarkLastFrame, 
									CCollisionHistory::ms_nHighWaterMark, 
									COLLISIONHISTORY_RECORDSTORESIZE, 
									sizeof(CCollisionRecord),
									(sizeof(CCollisionRecord) * COLLISIONHISTORY_RECORDSTORESIZE) / 1024,
									(sizeof(CCollisionRecord) * COLLISIONHISTORY_RECORDSTORESIZE + sizeof(int)) / 1024);
#endif
}

#ifdef _CPPRTTI
typedef struct EntityTransformCombo
{
	const rage::type_info* entity;
	u32 transform;

	bool operator==(const EntityTransformCombo& other) const {return (entity == other.entity && transform == other.transform);}
} EntityTransformCombo;

template <>
struct atMapHashFn<EntityTransformCombo>
{
	unsigned operator ()(EntityTransformCombo key) const { return atDataHash((char*)&key, sizeof(EntityTransformCombo)); }
};
#endif

void DisplayMatrixUsage()
{
#ifdef _CPPRTTI
	typedef atMap<EntityTransformCombo, int> ComboMap;
	ComboMap comboMap;

	CEntityIterator entityIterator(IterateAllTypes);

	typedef atMap<const rage::type_info*, u32> TypeMap;
	TypeMap uniqueTypes; 

	typedef atMap<u32, u32> TransformMap;
	TransformMap uniqueTransforms; 

	CEntity* nextEntity = entityIterator.GetNext();
	while (nextEntity)
	{
		const fwTransform& transform = nextEntity->GetTransform();

		EntityTransformCombo combo = { &typeid(*nextEntity), transform.GetTypeIdentifier() };

		uniqueTypes[combo.entity]++;
		uniqueTransforms[combo.transform]++;
		comboMap[combo]++;

		nextEntity = entityIterator.GetNext();
	}

	{
		CInteriorInst::Pool* pPool = CInteriorInst::GetPool();
		u32 numInteriors = CInteriorInst::GetPool()->GetNoOfUsedSpaces();
		for(u32 i=0; i<numInteriors; ++i){
			fwEntity* nextEntity = pPool->GetSlot(i);
			if (nextEntity)
			{
				const fwTransform& transform = nextEntity->GetTransform();
				EntityTransformCombo combo = { &typeid(*nextEntity), transform.GetTypeIdentifier() };

				uniqueTypes[combo.entity]++;
				uniqueTransforms[combo.transform]++;
				comboMap[combo]++;
			}
		}
	}

	{
		CPortalInst::Pool* pPool = CPortalInst::GetPool();
		u32 numPortals = pPool->GetSize();
		for(u32 i=0; i<numPortals; ++i){
			fwEntity* nextEntity = pPool->GetSlot(i);
			if (nextEntity)
			{
				const fwTransform& transform = nextEntity->GetTransform();
				EntityTransformCombo combo = { &typeid(*nextEntity), transform.GetTypeIdentifier() };

				uniqueTypes[combo.entity]++;
				uniqueTransforms[combo.transform]++;
				comboMap[combo]++;
			}
		}
	}


	const Color32 LABEL(0xffffff00);
	const Color32 COUNT(0xffffffff);
	const Color32 TOTAL(0xff000088);
	float xPos = 0.10f * (float) GRCDEVICE.GetWidth();
	float xColumn = 0.18f * (float) GRCDEVICE.GetWidth();
	float yPos = 0.2f * (float) GRCDEVICE.GetHeight();
	float yStep = (float) grcFont::GetCurrent().GetHeight();

	float x = xPos;
	float y = yPos;

	char infoString[32];

	const u32 debugDrawFlags = grcDebugDraw::FIXED_WIDTH | grcDebugDraw::NO_BG_QUAD | grcDebugDraw::RAW_COORDS;

	grcDebugDraw::PrintToScreenCoors("Matrix Stats", (s32) xPos, (s32) yPos, LABEL, debugDrawFlags);
	//TypeMap::Iterator typeIt = uniqueTypes.CreateIterator();

	// Transform labels.
	{
		TransformMap::Iterator transIt = uniqueTransforms.CreateIterator();

		while (transIt)
		{
			x += xColumn;
			formatf(infoString, "%s", fwTransform::GetTransformClassName((fwTransform::Type) transIt.GetKey()));
			grcDebugDraw::PrintToScreenCoors(infoString, (s32) x, (s32) y, LABEL, debugDrawFlags);

			transIt.Next();
		}

		x += xColumn;
		grcDebugDraw::PrintToScreenCoors("Total", (s32) x, (s32) y, LABEL, debugDrawFlags);
	}

	// Rows of type + count per trans + total.
	{
		TypeMap::Iterator typeIt = uniqueTypes.CreateIterator();

		y = yPos;

		while(typeIt)
		{
			y += yStep;
			x = xPos;
			formatf(infoString, "%s", ((::type_info*)typeIt.GetKey())->name());

			grcDebugDraw::PrintToScreenCoors(infoString, (s32) x, (s32) y, LABEL, debugDrawFlags);

			TransformMap::Iterator transIt = uniqueTransforms.CreateIterator();
			x += xColumn;

			while(transIt)
			{
				EntityTransformCombo combo = { typeIt.GetKey(), transIt.GetKey() };

				u32 count = comboMap[combo];

				formatf(infoString, "%d", count);
				grcDebugDraw::PrintToScreenCoors(infoString, (s32) x, (s32) y, COUNT, debugDrawFlags);

				x += xColumn;
				transIt.Next();
			}

			formatf(infoString, "%d", typeIt.GetData());

			grcDebugDraw::PrintToScreenCoors(infoString, (s32) x, (s32) y, TOTAL, debugDrawFlags);

			typeIt.Next();
		}

		// Transform per type
		y += yStep;
		x = xPos;
		grcDebugDraw::PrintToScreenCoors("Totals", (s32) x, (s32) y, LABEL, debugDrawFlags);
		x += xColumn;
	}

	{
		// Totals per transform
		TransformMap::Iterator transIt = uniqueTransforms.CreateIterator();

		while(transIt)
		{
			formatf(infoString, "%d", transIt.GetData());
			grcDebugDraw::PrintToScreenCoors(infoString, (s32) x, (s32) y, TOTAL, debugDrawFlags);

			x += xColumn;
			transIt.Next();
		}
	}
#endif
}

//
// name:		DisplayStoreUsage
// description:	Display how much of each store is used
void DisplayStoreUsage()
{
    grcDebugDraw::AddDebugOutputEx(false,"name                              used/total");

    g_storesTrackingArray.QSort(0, -1, CompareStoreArray);
    for (s32 i=0; i<g_storesTrackingArray.GetCount(); i++)
    {
        //NOTE: this is ok because we are not attempting to access any of the data in the store
        //  just the variables of the store.
        fwAssetStore<char>* temp = (fwAssetStore<char>*)g_storesTrackingArray[i];
        grcDebugDraw::AddDebugOutputEx(false,"%32s %5d/%5d", temp->GetModuleName(), temp->GetNumUsedSlots(), temp->GetSize());
    }
}

//
// name:		DisplayPackfileUsage
// description:	Display how much of the packfile reserve is used
void DisplayPackfileUsage()
{
    grcDebugDraw::AddDebugOutputEx(false,"name           used/total");
    grcDebugDraw::AddDebugOutputEx(false,"ArchiveCount  %5d/%5d", strPackfileManager::GetFileArraySize(), strPackfileManager::GetFileArrayCapacity());
}

void CStreamingDebug::UpdatePageSizeStringToAllocate()
{
	size_t size = pgRscBuilder::ComputeLeafSize(1 << sPageSizeToAllocate, !sPageToAllocateIsVirtual);
	formatf(sPageSizeToAllocateStr, "%dKB", (int) (size / 1024));
}

void CStreamingDebug::AllocateResourceMemoryPage()
{
	MEM_USE_USERDATA(MEMUSERDATA_DEBUGSTEAL);
	size_t size = pgRscBuilder::ComputeLeafSize(1 << sPageSizeToAllocate, !sPageToAllocateIsVirtual);

	void *block = strStreamingEngine::GetAllocator().Allocate(size, 0, (sPageToAllocateIsVirtual) ? MEMTYPE_RESOURCE_VIRTUAL : MEMTYPE_RESOURCE_PHYSICAL);

	if (block)
	{
		sAllocatedPages.Grow() = block;
		sAllocatedPageSizes.Grow() = size | ((sPageToAllocateIsVirtual) ? 0x80000000 : 0);
#if PROTECT_STOLEN_MEMORY
		if (!g_DisableReadOnly)
		{
			ProtectRange(block, size, true);
		}
#endif
	}
	else
	{
		streamErrorf("Could not allocate %" SIZETFMT "dKB", size);
	}
}

void CStreamingDebug::AllocateBlocksToFragmentMemory()
{
	MEM_USE_USERDATA(MEMUSERDATA_DEBUGSTEAL);

	size_t size = pgRscBuilder::ComputeLeafSize(1 << sPageSizeToAllocate, !sPageToAllocateIsVirtual);
	size_t packedSize = size | ((sPageToAllocateIsVirtual) ? 0x80000000 : 0);

	// To fragment everything past a certain size, we have to allocate all pages one size
	// smaller and release every other block.
	size >>= 1;

	atArray<void *> tempAllocatedBlocks;

	while (true)
	{
		void *block = strStreamingEngine::GetAllocator().Allocate(size, 0, (sPageToAllocateIsVirtual) ? MEMTYPE_RESOURCE_VIRTUAL : MEMTYPE_RESOURCE_PHYSICAL);

		if (block)
		{
			tempAllocatedBlocks.Grow(32) = block;
#if PROTECT_STOLEN_MEMORY
			if (!g_DisableReadOnly)
			{
				ProtectRange(block, size, true);
			}
#endif
		}
		else
		{
			// No more blocks of that size available.
			break;
		}
	}

	// Now that we allocated EVERYTHING of that block size, free up every other block.
	int blockCursor = 0;
	int blockCount = tempAllocatedBlocks.GetCount();

	while (blockCursor < blockCount)
	{
		// Let's look at this block. Did we allocate its neighbor too? If so, allocate the neighbor and
		// permanently keep this one around.

		char *block = (char *) tempAllocatedBlocks[blockCursor];
		char *neighbor = block + size;
		bool foundNeighbor = false;

		for (int x=0; x<blockCount; x++)
		{
			if (tempAllocatedBlocks[x] == neighbor)
			{
				// We have the neighbor! Free it up, keep this one for good.
				sAllocatedPages.Grow() = block;
				sAllocatedPageSizes.Grow() = packedSize;

				// Remove it from the list of allocated blocks if it was there already.
				int neighborIndex = sAllocatedPages.Find(neighbor);

				if (neighborIndex != -1)
				{
					sAllocatedPages.DeleteFast(neighborIndex);
					sAllocatedPageSizes.DeleteFast(neighborIndex);
				}

#if PROTECT_STOLEN_MEMORY
				if (!g_DisableReadOnly)
				{
					ProtectRange(neighbor, size, false);
				}
#endif

				strStreamingEngine::GetAllocator().Free(neighbor);

				// Remove those two blocks from the temp pool, we're done with them.
				if (x > blockCursor)
				{
					tempAllocatedBlocks.DeleteFast(x);
					blockCount--;
				}
				tempAllocatedBlocks.DeleteFast(blockCursor);
				blockCount--;
				foundNeighbor = true;
				break;
			}
		}

		if (!foundNeighbor)
		{
			// No neighbor for this one. Not sure whether or not we should free it up, but let's be dicks
			// and hold on to it.
			sAllocatedPages.Grow() = block;
			sAllocatedPageSizes.Grow() = packedSize;
			blockCursor++;
//			tempAllocatedBlocks.DeleteFast(blockCursor);
//			blockCount--;
		}
	}
}

void CStreamingDebug::FreeAllAllocatedResourceMemoryPages()
{
	MEM_USE_USERDATA(MEMUSERDATA_DEBUGSTEAL);
	for (int x=0; x<sAllocatedPages.GetCount(); x++)
	{
#if PROTECT_STOLEN_MEMORY
		if (!g_DisableReadOnly)
		{
			ProtectRange(sAllocatedPages[x], sysMemAllocator::GetMaster().GetSize(sAllocatedPages[x]), false);
		}
#endif
		strStreamingEngine::GetAllocator().Free(sAllocatedPages[x]);
	}

	sAllocatedPages.Resize(0);
	sAllocatedPageSizes.Resize(0);
}

#if __DEV

void CStreamingDebug::DumpRequests()
{
	pgStreamer::DumpRequests();
}

void CStreamingDebug::VMapSceneStreaming()
{
	// CSceneStreamerDebug::SetDrawRequestsOnVmap(true);
	g_show_streaming_on_vmap = true;
}

void CStreamingDebug::WriteLargeResources()
{
	strStreamingInfoManager& infoManager = strStreamingEngine::GetInfo();

	strStreamingInfoIterator it;
	while (!it.IsAtEnd())
	{
		strIndex index = *it;
		strStreamingInfo* pInfo = infoManager.GetStreamingInfo(index);

		int virtSize, physSize;
		GetSizesSafe(index, *pInfo, virtSize, physSize, false);

		u32 size = virtSize + physSize;

		if ((size != 0) && size > (s_largeResourceSize * 1024 * 1024))
		{
			const char* packfileName = strPackfileManager::GetImageFileNameFromHandle(pInfo->GetHandle());

			streamDisplayf("%s/%s - %u MB (%u bytes)", packfileName, strStreamingEngine::GetObjectName(index), size / (1024 * 1024), size);
		}

		++it;
	}
}

void CalculateGeometryBufferSizes(rmcDrawable* drawable, size_t& vertexCount, size_t& vertexBufferSize, size_t& indexBufferSize)
{
	const rmcLodGroup& lodGroup = drawable->GetLodGroup();

	vertexBufferSize = 0;
	indexBufferSize = 0;
	vertexCount = 0;

	int lodIndex=0;
	while (lodGroup.ContainsLod(lodIndex))
	{
		const rmcLod& lod = lodGroup.GetLod(lodIndex);

		int modelCount = lod.GetCount();
		for (int i=0; i<modelCount; ++i)
		{
			grmModel* model = lod.GetModel(i);

			int geomCount = model->GetGeometryCount();
			for (int j=0; j<geomCount; ++j)
			{
				grmGeometry& geometry = model->GetGeometry(j);

				grcVertexBuffer* vertexBuffer = geometry.GetVertexBuffer();
				if(vertexBuffer)
				{
					vertexBufferSize += vertexBuffer->GetVertexStride() * vertexBuffer->GetVertexCount();
					vertexCount += vertexBuffer->GetVertexCount();
				}

				grcIndexBuffer* indexBuffer = geometry.GetIndexBuffer();
				if(indexBuffer)
				{
					indexBufferSize += indexBuffer->GetIndexCount() * sizeof(u16);
				}
			}
		}

		lodIndex++;
	}
}

//
// name:		SanityCheckMemoryHeaps
// description:	Run Sanity Check on all the heaps
void SanityCheckMemoryHeaps()
{
	sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL)->SanityCheck();
	sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_VIRTUAL)->SanityCheck();
#if !ONE_STREAMING_HEAP
	sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_PHYSICAL)->SanityCheck();
	sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_PHYSICAL)->SanityCheck();
#endif
}
#endif // __DEV

#define ROUND(value, power)	(((value + ((1<<(power))-1)) & ~((1<<(power))-1)) >> (power))

void WriteBucketUsage(fiStream* s, const char* pName, s32 bucket)
{
#if !ONE_STREAMING_HEAP
	fprintf(s,"%s, %d, %d, %d, %d\n", pName,
		(s32)sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL)->GetMemoryUsed(bucket)/1024,
		(s32)sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_VIRTUAL)->GetMemoryUsed(bucket)/1024,
		(s32)sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_PHYSICAL)->GetMemoryUsed(bucket)/1024,
		(s32)sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_PHYSICAL)->GetMemoryUsed(bucket)/1024);
#else
	fprintf(s,"%s %d, %d\n", pName,
		(s32)sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL)->GetMemoryUsed(bucket)/1024,
		(s32)sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_VIRTUAL)->GetMemoryUsed(bucket)/1024);
#endif
}

void WriteMemoryUsage(fiStream* s)
{
	sysMemAllocator* pAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL);

#if !ONE_STREAMING_HEAP
	fprintf(s, "Game Virtual, %d, %d, %d\n",
		(s32)(pAllocator->GetMemoryAvailable() + pAllocator->GetMemoryUsed())/1024,
		(s32)pAllocator->GetMemoryUsed()/1024,
		(s32)pAllocator->GetMemoryAvailable()/1024);
	pAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_VIRTUAL);
	fprintf(s, "Resource Virtual, %d, %d, %d\n",
		(s32)(pAllocator->GetMemoryAvailable() + pAllocator->GetMemoryUsed())/1024,
		(s32)pAllocator->GetMemoryUsed()/1024,
		(s32)pAllocator->GetMemoryAvailable()/1024);
	pAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_PHYSICAL);
	fprintf(s, "Game Physical, %d, %d, %d\n",
		(s32)(pAllocator->GetMemoryAvailable() + pAllocator->GetMemoryUsed())/1024,
		(s32)pAllocator->GetMemoryUsed()/1024,
		(s32)pAllocator->GetMemoryAvailable()/1024);
	pAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_PHYSICAL);
	fprintf(s, "Resource Physical, %d, %d, %d\n",
		(s32)(pAllocator->GetMemoryAvailable() + pAllocator->GetMemoryUsed())/1024,
		(s32)pAllocator->GetMemoryUsed()/1024,
		(s32)pAllocator->GetMemoryAvailable()/1024);

#if __PPU
	if (g_pResidualAllocator)
	{
		pAllocator = g_pResidualAllocator;
		fprintf(s, "Residual Allocator, %d, %d, %d\n",
			(s32)(pAllocator->GetMemoryAvailable() + pAllocator->GetMemoryUsed())/1024,
			(s32)pAllocator->GetMemoryUsed()/1024,
			(s32)pAllocator->GetMemoryAvailable()/1024);
	}
#endif // __PPU
	
#else
	fprintf(s, "Game, %d, %d, %d\n",
		(s32)(pAllocator->GetMemoryAvailable() + pAllocator->GetMemoryUsed())/1024,
		(s32)pAllocator->GetMemoryUsed()/1024,
		(s32)pAllocator->GetMemoryAvailable()/1024);
	pAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_VIRTUAL);
	fprintf(s, "Physical, %d, %d, %d\n",
		(s32)(pAllocator->GetMemoryAvailable() + pAllocator->GetMemoryUsed())/1024,
		(s32)pAllocator->GetMemoryUsed()/1024,
		(s32)pAllocator->GetMemoryAvailable()/1024);
#endif // !ONE_STREAMING_HEAP

#if ENABLE_DEBUG_HEAP
	pAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_DEBUG_VIRTUAL);
	fprintf(s, "Debug, %d, %d, %d\n",
		(s32)(pAllocator->GetMemoryAvailable() + pAllocator->GetMemoryUsed())/1024,
		(s32)pAllocator->GetMemoryUsed()/1024,
		(s32)pAllocator->GetMemoryAvailable()/1024);
#endif
}

void WriteModuleMemory(fiStream* s)
{
	u32 moduleVirtualMemSize[MAX_STREAMING_MODULES];
	u32 modulePhysicalMemSize[MAX_STREAMING_MODULES];

	u32 moduleVirtualMemSizeInUse[MAX_STREAMING_MODULES];
	u32 modulePhysicalMemSizeInUse[MAX_STREAMING_MODULES];

	u32 moduleVirtualMemSizeTotals[MAX_STREAMING_MODULES];
	u32 modulePhysicalMemSizeTotals[MAX_STREAMING_MODULES];

	s32 moduleId;

	// clear array
	for(moduleId=0; moduleId<MAX_STREAMING_MODULES; moduleId++)
	{
		moduleVirtualMemSize[moduleId] = 0;
		modulePhysicalMemSize[moduleId] = 0;
		moduleVirtualMemSizeInUse[moduleId] = 0;
		modulePhysicalMemSizeInUse[moduleId] = 0;
		moduleVirtualMemSizeTotals[moduleId] = 0;
		modulePhysicalMemSizeTotals[moduleId] = 0;
	}

	// count up memory for each module while ignoring extra loaders
	for(moduleId=0; moduleId < strStreamingEngine::GetInfo().GetModuleMgr().GetNumberOfModules(); moduleId++)
	{
		strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(moduleId);

		// add up memory
		for(s32 obj = 0; obj<pModule->GetCount(); obj++)
		{
			strIndex index = pModule->GetStreamingIndex(strLocalIndex(obj));
			strStreamingInfo* info = strStreamingEngine::GetInfo().GetStreamingInfo(index);
			if (info->IsInImage())
			{
				int virtualSize;
				int physicalSize;
				GetLoadedSizesSafe(*pModule, strLocalIndex(obj), virtualSize, physicalSize);

				moduleVirtualMemSizeTotals[moduleId] += virtualSize;
				modulePhysicalMemSizeTotals[moduleId] += physicalSize;

				if (info->GetStatus() == STRINFO_LOADED)
				{
					moduleVirtualMemSize[moduleId] += virtualSize;
					modulePhysicalMemSize[moduleId] += physicalSize;

					if (info->IsFlagSet(STRFLAG_DONTDELETE) || strStreamingEngine::GetInfo().IsObjectInUse(index))
					{
						moduleVirtualMemSizeInUse[moduleId] += virtualSize;
						modulePhysicalMemSizeInUse[moduleId] += physicalSize;
					}
				}
			}
			
		}
	}

	// Print out results
	for(moduleId = 0; moduleId < MAX_STREAMING_MODULES-1; moduleId++)
	{
		if(moduleVirtualMemSize[moduleId] != 0 || modulePhysicalMemSize[moduleId] != 0)
		{
			fprintf(s, "%s, %d, %d, %d, %d, %d, %d\n",
				strStreamingEngine::GetInfo().GetModuleMgr().GetModule(moduleId)->GetModuleName(),
				moduleVirtualMemSize[moduleId]>>10, modulePhysicalMemSize[moduleId]>>10,
				moduleVirtualMemSizeInUse[moduleId]>>10, modulePhysicalMemSizeInUse[moduleId]>>10,
				moduleVirtualMemSizeTotals[moduleId]>>10, modulePhysicalMemSizeTotals[moduleId]>>10);
		}
	}
}

void WriteRequests(fiStream* s)
{
	u32 virtualSize[MAX_STREAMING_MODULES];
	u32 physicalSize[MAX_STREAMING_MODULES];

	memset(virtualSize, 0, sizeof(virtualSize));
	memset(physicalSize, 0, sizeof(physicalSize));

	RequestInfoList::Iterator it(strStreamingEngine::GetInfo().GetRequestInfoList()->GetFirst());

	while (it.IsValid())
	{		
		strIndex index = it.Item()->m_Index;
		strStreamingInfo& info = strStreamingEngine::GetInfo().GetStreamingInfoRef(index);

		int virtSize, physSize;
		GetSizesSafe(index, info, virtSize, physSize, false);

		u8 module = strStreamingEngine::GetInfo().GetModuleMgr().GetModuleIdFromIndex(index);

		virtualSize[module] += virtSize;
		physicalSize[module] += physSize;
		it.Next();
	}

	for (int i = 0; i < MAX_STREAMING_MODULES; ++i)
	{
		if (virtualSize[i] || physicalSize[i])
			fprintf(s, "%s, %d, %d\n", strStreamingEngine::GetInfo().GetModuleMgr().GetModule(i)->GetModuleName(), virtualSize[i]>>10, physicalSize[i] >>10);
	}
}

void WriteMemoryReport()
{
	fiStream* s = fiStream::Create("x:/memoryreport.csv");

	if (s)
	{
		fprintf(s,"==== Allocators ====\n");
		WriteMemoryUsage(s);
		fprintf(s,"==== Buckets ====\n");
		WriteBucketUsage(s,"Default", MEMBUCKET_DEFAULT);
		WriteBucketUsage(s,"Animation", MEMBUCKET_ANIMATION);
		WriteBucketUsage(s,"Streaming", MEMBUCKET_STREAMING);
		WriteBucketUsage(s,"World", MEMBUCKET_WORLD);
		WriteBucketUsage(s,"Gameplay", MEMBUCKET_GAMEPLAY);
		WriteBucketUsage(s,"FX", MEMBUCKET_FX);
		WriteBucketUsage(s,"Rendering", MEMBUCKET_RENDER);
		WriteBucketUsage(s,"Physics", MEMBUCKET_PHYSICS);
		WriteBucketUsage(s,"Audio", MEMBUCKET_AUDIO);
		WriteBucketUsage(s,"Network", MEMBUCKET_NETWORK);
		WriteBucketUsage(s,"System", MEMBUCKET_SYSTEM);
		WriteBucketUsage(s,"Scaleform", MEMBUCKET_SCALEFORM);
		WriteBucketUsage(s,"Script", MEMBUCKET_SCRIPT);
		WriteBucketUsage(s,"Resource", MEMBUCKET_RESOURCE);
		WriteBucketUsage(s,"Debug", MEMBUCKET_DEBUG);
		fprintf(s,"==== Streaming ====\n");
		WriteModuleMemory(s);
		fprintf(s, "==== Requests ====\n");
		WriteRequests(s);
#if __PPU
		fprintf(s, "==== System usage ====\n");
		struct std::malloc_managed_size mms;
		std::malloc_stats(&mms);
		fprintf(s, "System usage, %d, %d", mms.current_system_size>>10,mms.current_inuse_size>>10);
#endif

		s->Close();
	}
}

void CStreamingDebug::OutputLine(fiStream *stream, bool toScreen, Color32 color, const char *line)
{
	char finalLine[256];

	// Format the line for the output device.
	int count = sizeof(finalLine) - 1;
	// Column separator. This is \t in TTY/file output (so it can be pasted into Excel), and a
	// single space on the screen.
	const char separator = (toScreen) ? ' ' : '\t';
	const char *srcLine = line;
	char *dstLine = finalLine;

	while (count-- && *srcLine)
	{
		// Look for column separators.
		if (*srcLine == '|')
		{
			*dstLine = separator;
		}
		else
		{
			*dstLine = *srcLine;
		}

		srcLine++;
		dstLine++;
	}

	// Terminate the final string.
	*dstLine = 0;

	if (toScreen)
	{
		grcDebugDraw::AddDebugOutputEx(false, color, "%s", finalLine);
	}
	else
	{
		if (stream)
		{
			//int stringLength = sizeof(finalLine) - count + 1;
			const int stringLength = istrlen(finalLine);
			stream->Write(finalLine, stringLength);
			stream->Write("\r\n", 2);
		}
		else
		{
			streamDisplayf("%s", finalLine);
		}
	}
}

#endif // __BANK

//////////////////////////////////////////////////////////////////////////
#if  0 // some code for KS to measure memory in final etc.
namespace rage {
	extern void CheckpointSystemMemory(const char *file,int line);
}

void CalculateGeometryQBBufferSizes(rmcDrawable* drawable, u32& vertexCount)
{
	const rmcLodGroup& lodGroup = drawable->GetLodGroup();

	vertexCount = 0;

	int lodIndex=0;
	while (lodGroup.ContainsLod(lodIndex))
	{
		const rmcLod& lod = lodGroup.GetLod(lodIndex);

		int modelCount = lod.GetCount();
		for (int i=0; i<modelCount; ++i)
		{
			grmModel* model = lod.GetModel(i);

			int geomCount = model->GetGeometryCount();
			for (int j=0; j<geomCount; ++j)
			{
				grmGeometry& geometry = model->GetGeometry(j);

				if (geometry.GetType() == grmGeometry::GEOMETRYEDGE)
					vertexCount += geometry.GetVertexCount();
			}
		}

		lodIndex++;
	}
}

void WriteGeometryQBDataSizes()
{
	size_t vertexCount;
	size_t vertexCountTotal[17] = {0};

	const int maxModelInfos = CModelInfo::GetMaxModelInfos();
	for (u32 model=0; model<maxModelInfos; ++model)
	{
		CBaseModelInfo* modelInfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(model)));

		if (modelInfo && modelInfo->GetDrawDictIndex() == INVALID_DRAWDICT_IDX)
		{
			rmcDrawable* drawable = modelInfo->GetDrawable();

			if (drawable)
			{
				CalculateGeometryQBBufferSizes(drawable, vertexCount);

				const u8 modelType = modelInfo->GetModelType();
				if (streamVerify(modelType < 8))
				{
					vertexCountTotal[modelType] += vertexCount;
				}
			}
		}
	}

	for (int dict = 0; dict<g_DwdStore.GetSize(); ++dict)
	{
		if (g_DwdStore.IsValidSlot(dict))
		{
			Dwd* drawableDict = g_DwdStore.Get(dict);

			if (drawableDict)
			{
				int category = 16;

				for (u32 info=0; info<maxModelInfos; ++info)
				{
					CBaseModelInfo* modelInfo = CModelInfo::GetBaseModelInfo(fwModelId(info));

					if (modelInfo && (modelInfo->GetModelType() != MI_TYPE_PED?modelInfo->GetDrawDictIndex() == dict: static_cast<CPedModelInfo*>(modelInfo)->GetPedComponentFileIndex() == dict))
					{
						const u8 modelType = modelInfo->GetModelType();
						streamAssert(modelType < 8);
						category = modelType + 8;
						break;
					}
				}

				for (int model=0; model<drawableDict->GetCount(); ++model)
				{
					rmcDrawable* drawable = drawableDict->GetEntry(model);

					if (drawable)
					{
						CalculateGeometryQBBufferSizes(drawable, vertexCount);
						vertexCountTotal[category] += vertexCount;
					}
				}
			}
		}
	}

	const char * names[17] = {
		"MI_TYPE_NONE",
		"MI_TYPE_BASE",
		"MI_TYPE_MLO",
		"MI_TYPE_TIME",
		"MI_TYPE_WEAPON",
		"MI_TYPE_VEHICLE",
		"MI_TYPE_PED",
		"MI_TYPE_TREE",
		"DR_TYPE_NONE",
		"DR_TYPE_BASE",
		"DR_TYPE_MLO",
		"DR_TYPE_TIME",
		"DR_TYPE_WEAPON",
		"DR_TYPE_VEHICLE",
		"DR_TYPE_PED",
		"DR_TYPE_TREE",
		"DRAWABLEDICT"
	};

	int totalVerts = 0;
	for (int i = 0; i<17; ++i)
	{
		streamDisplayf("Type %s - Vertex count index %d", names[i], vertexCountTotal[i]);
		totalVerts += vertexCountTotal[i];
	}

	streamDisplayf("Total - Vertex count index %d", totalVerts);
}

void WriteFinalMemoryUsage()
{
	sysMemAllocator* pAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL);

	printf( "Game Virtual, %d, %d, %d\n",
		(s32)(pAllocator->GetMemoryAvailable() + pAllocator->GetMemoryUsed())/1024,
		(s32)pAllocator->GetMemoryUsed()/1024,
		(s32)pAllocator->GetMemoryAvailable()/1024);
	pAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_VIRTUAL);
	printf( "Resource Virtual, %d, %d, %d\n",
		(s32)(pAllocator->GetMemoryAvailable() + pAllocator->GetMemoryUsed())/1024,
		(s32)pAllocator->GetMemoryUsed()/1024,
		(s32)pAllocator->GetMemoryAvailable()/1024);

	int virtualAvailable = pAllocator->GetMemoryAvailable();
	int virtualStreaming = (strStreamingEngine::GetAllocator().GetVirtualMemoryAvailable() - strStreamingEngine::GetAllocator().GetVirtualMemoryUsed());
	int optimalVirtual =  virtualAvailable - 5 * 1024 * 1024 - virtualStreaming;  


	pAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_PHYSICAL);
	printf( "Game Physical, %d, %d, %d\n",
		(s32)(pAllocator->GetMemoryAvailable() + pAllocator->GetMemoryUsed())/1024,
		(s32)pAllocator->GetMemoryUsed()/1024,
		(s32)pAllocator->GetMemoryAvailable()/1024);
	pAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_PHYSICAL);
	printf( "Resource Physical, %d, %d, %d\n",
		(s32)(pAllocator->GetMemoryAvailable() + pAllocator->GetMemoryUsed())/1024,
		(s32)pAllocator->GetMemoryUsed()/1024,
		(s32)pAllocator->GetMemoryAvailable()/1024);

	int physicalAvailable = pAllocator->GetMemoryAvailable();
	int physicalStreaming = (s32)(strStreamingEngine::GetAllocator().GetPhysicalMemoryAvailable() - strStreamingEngine::GetAllocator().GetPhysicalMemoryUsed());
	int optimalPhysical = physicalAvailable - 5 * 1024 * 1024 - physicalStreaming;  

	if (g_pResidualAllocator)
	{
		pAllocator = g_pResidualAllocator;
		printf( "Residual Allocator2, %d, %d, %d\n",
			(s32)(pAllocator->GetMemoryAvailable() + pAllocator->GetMemoryUsed())/1024,
			(s32)pAllocator->GetMemoryUsed()/1024,
			(s32)pAllocator->GetMemoryAvailable()/1024);
	}

	printf("Virtual streaming memory %dK (%dK -> %dK)\n",
		(s32)strStreamingEngine::GetAllocator().GetVirtualMemoryUsed()/1024,
		(s32)strStreamingEngine::GetAllocator().GetVirtualMemoryAvailable()/1024,
		(s32)(strStreamingEngine::GetAllocator().GetVirtualMemoryAvailable() - strStreamingEngine::GetAllocator().GetVirtualMemoryUsed())/1024
		);
	printf("Physical streaming memory %dK (%dK -> %dK)\n",
		(s32)strStreamingEngine::GetAllocator().GetPhysicalMemoryUsed()/1024,
		(s32)strStreamingEngine::GetAllocator().GetPhysicalMemoryAvailable()/1024,
		(s32)(strStreamingEngine::GetAllocator().GetPhysicalMemoryAvailable() - strStreamingEngine::GetAllocator().GetPhysicalMemoryUsed())/1024);

	printf("\nOptimal virtual %d (%d) / physical %d (%d)\n", 
		(strStreamingEngine::GetAllocator().GetVirtualMemoryAvailable() + optimalVirtual) >> 10, optimalVirtual >> 10,
		(strStreamingEngine::GetAllocator().GetPhysicalMemoryAvailable() + optimalPhysical) >> 10, optimalPhysical >> 10);

#if __PPU
	struct std::malloc_managed_size mms;
	std::malloc_stats(&mms);
	printf("**** current system usage %dK; in use is %dk\n",mms.current_system_size>>10,mms.current_inuse_size>>10);
#endif

	CheckpointSystemMemory(__FILE__, __LINE__);

	// WriteGeometryQBDataSizes();
}

#endif // KS
//////////////////////////////////////////////////////////////////////////

size_t CStreamingDebug::GetMainMemoryUsed(const int bucket) 
{
	sysMemSimpleAllocator* pGameVirtual = static_cast<sysMemSimpleAllocator*>(sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL));
	Assert(pGameVirtual);

#if __PPU
	sysMemSimpleAllocator* pFlexAllocator = static_cast<sysMemSimpleAllocator*>(sysMemManager::GetInstance().GetFlexAllocator()->GetPrimaryAllocator());
	Assert(pFlexAllocator);

	sysMemSimpleAllocator* pResidualAllocator = static_cast<sysMemSimpleAllocator*>(g_pResidualAllocator);
	Assert(pResidualAllocator);

	const size_t items = 3;
	sysMemSimpleAllocator* pAllocator[items] = {pGameVirtual, pFlexAllocator, pResidualAllocator};
#elif __XENON
	sysMemSimpleAllocator* pPoolAllocator = static_cast<sysMemSimpleAllocator*>(g_pXenonPoolAllocator);
	Assert(pPoolAllocator);

	const size_t items = 2;
	sysMemSimpleAllocator* pAllocator[items] = {pGameVirtual, pPoolAllocator};
#else
	const size_t items = 1;
	sysMemSimpleAllocator* pAllocator[items] = {pGameVirtual};
#endif

	size_t bytes = 0;
	for (size_t i = 0; i < items; ++i)
		bytes += pAllocator[i]->GetMemoryUsed(bucket);
	
	return bytes;
}

size_t CStreamingDebug::GetMainMemoryAvailable()
{
	sysMemSimpleAllocator* pGameVirtual = static_cast<sysMemSimpleAllocator*>(sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL));
	Assert(pGameVirtual);

#if __PPU
	sysMemSimpleAllocator* pFlexAllocator = static_cast<sysMemSimpleAllocator*>(sysMemManager::GetInstance().GetFlexAllocator()->GetPrimaryAllocator());
	Assert(pFlexAllocator);

	sysMemSimpleAllocator* pResidualAllocator = static_cast<sysMemSimpleAllocator*>(g_pResidualAllocator);
	Assert(pResidualAllocator);

	const size_t items = 3;
	sysMemSimpleAllocator* pAllocator[items] = {pGameVirtual, pFlexAllocator, pResidualAllocator};
#elif __XENON && ENABLE_DEBUG_HEAP
	sysMemSimpleAllocator* pPoolAllocator = static_cast<sysMemSimpleAllocator*>(g_pXenonPoolAllocator);
	Assert(pPoolAllocator);

	const size_t items = 2;
	sysMemSimpleAllocator* pAllocator[items] = {pGameVirtual, pPoolAllocator};
#else
	const size_t items = 1;
	sysMemSimpleAllocator* pAllocator[items] = {pGameVirtual};
#endif

	size_t bytes = 0;
	for (size_t i = 0; i < items; ++i)
		bytes += pAllocator[i]->GetMemoryAvailable();

	return bytes;
}

#if !__WIN32PC && DEBUG_DRAW
void CStreamingDebug::ShowLowLevelStreaming()
{
	float xPosLSN = ((float) GRCDEVICE.GetWidth()) / 2.0f - 200.0f;
	//float xPosSeek = xPosLSN + 60.0f;
	float xPosFile = xPosLSN + 488.0f;

	float yPos = 110.0f;
	float height = (float) (grcFont::GetCurrent().GetHeight() + 1);

	int start = s_Start;
	int end = s_End;

	static s32 opticalLsn;

	static float lastThroughput = 0.f;
	float totalRecordedDuration = 0.f;
	u32 totalRecordedSize = 0;

	const u32 debugDrawFlags = grcDebugDraw::FIXED_WIDTH | grcDebugDraw::NO_BG_QUAD | grcDebugDraw::RAW_COORDS;

	char line[256];
	formatf(line, "%8s%8s%8s%8s%8s%8s%7s %-4s %s", "Size", "Dur", "Read", "Def", "Prio", "Time", "Seek", "Layr", "Name");
	grcDebugDraw::PrintToScreenCoors(line, (s32) xPosLSN, (s32) yPos, Color_white, debugDrawFlags);
	yPos += height * 1.5f;

	while (start != end)
	{
		const pgStreamerRequestDebugInfo &debugInfo = s_DebugInfo[start];

		if (debugInfo.IsValid())
		{
			bool hdd = (debugInfo.m_LSN >> 31) != 0;
			if (hdd)
			{
				formatf(line, "%8d%8d%8d%8d%8.2f%8.2f        %s", debugInfo.m_Size, debugInfo.m_Duration, debugInfo.m_Read, debugInfo.m_Deflate, debugInfo.m_Priority, debugInfo.m_Time,  "hdd");
				grcDebugDraw::PrintToScreenCoors(line, (s32) xPosLSN, (s32) yPos, Color32(0xff0000cf), debugDrawFlags);
			}
			else
			{
				s32 newLsn = (s32) (debugInfo.m_LSN & 0x3FFFFFFF);
				bool oldLayer = (opticalLsn >= fiDevice::LAYER1_LSN);
				bool newLayer = (newLsn >= fiDevice::LAYER1_LSN);

				s32 dist = newLsn - opticalLsn;
				if (oldLayer != newLayer)	// this can get weird on "oversized" emulation images.
					dist = newLsn > opticalLsn? (fiDevice::LAYER1_LSN + fiDevice::LAYER1_LSN - newLsn) - opticalLsn: (fiDevice::LAYER1_LSN + fiDevice::LAYER1_LSN - opticalLsn) - newLsn;

				dist = abs(dist);
				if (dist > 999999) dist = 999999;	// clamp to something legible

				opticalLsn = newLsn;

				formatf(line, "%8d%8d%8d%8d%8.2f%8.2f%7d %s", debugInfo.m_Size, debugInfo.m_Duration, debugInfo.m_Read, debugInfo.m_Deflate, debugInfo.m_Priority, debugInfo.m_Time, dist, newLayer? "dv1" : "dv0");
				Color32 color = Color32(oldLayer != newLayer? 0xffcf0000 : (abs(dist) > 10000) ? 0xffcfcf00 : 0xff00cfcf);
				grcDebugDraw::PrintToScreenCoors(line, (s32) xPosLSN, (s32) yPos, color, debugDrawFlags);

				totalRecordedDuration += debugInfo.m_Duration / 1000.f;
				totalRecordedSize += debugInfo.m_Size >> 10;
			}

			grcDebugDraw::PrintToScreenCoors(debugInfo.m_FileName, (s32) xPosFile, (s32) yPos, Color32(Color_white), debugDrawFlags);

			yPos += height;
		}

		if (++start == s_DebugInfo.GetMaxCount())
			start = 0;
	}

	yPos += height * 0.5f;
	formatf(line, "Last recorded: %d Kb streamed in %.2f seconds", totalRecordedSize, totalRecordedDuration);
	grcDebugDraw::PrintToScreenCoors(line, (s32) xPosLSN, (s32) yPos, Color_white, debugDrawFlags);
	yPos += height;
	formatf(line, "Last recorded throughput %.2f Kb/s", totalRecordedSize / totalRecordedDuration);
	grcDebugDraw::PrintToScreenCoors(line, (s32) xPosLSN, (s32) yPos, Color_white, debugDrawFlags);
	yPos += height;
	lastThroughput = lastThroughput * 0.9f + (totalRecordedSize / totalRecordedDuration) * 0.1f;
	formatf(line, "Average throughput %.2f Kb/s", lastThroughput);
	grcDebugDraw::PrintToScreenCoors(line, (s32) xPosLSN, (s32) yPos, Color_white, debugDrawFlags);
}
#endif // !__WIN32PC && DEBUG_DRAW

#if STREAMING_VISUALIZE
static void s_DebugTracking(const char * filename,u32 lsn,u32 size,float prio,u32 duration,u32 read,u32 deflate,int STRVIS_ONLY(device), u32 /*handle*/, u32 STRVIS_ONLY(readId))
{
	STRVIS_FINISH_REQUEST(device, readId);

#if DEBUG_DRAW && !__WIN32PC
	SYS_CS_SYNC(s_DebugTrackingCs);

	pgStreamerRequestDebugInfo& info = s_DebugInfo[s_End];
	safecpy(info.m_FileName,ASSET.FileName(filename));
	info.m_LSN = lsn;
	info.m_Time = debugTimer.GetTime();
	info.m_Priority = prio;
	info.m_Size = size;
	info.m_Duration = duration; 
	info.m_Read = read; 
	info.m_Deflate = deflate; 
	if (++s_End == s_DebugInfo.GetMaxCount())
		s_End = 0;
	if (s_Start == s_End && ++s_Start == s_DebugInfo.GetMaxCount())
		s_Start = 0;
#else // DEBUG_DRAW && !__WIN32PC
	(void)filename;
	(void)lsn;
	(void)size;
	(void)prio;
	(void)duration;
	(void)read;
	(void)deflate;
#endif // DEBUG_DRAW && !__WIN32PC
}

static void s_DebugBeginTracking(const char * /*filename*/,u32 /*lsn*/,u32 /*size*/,float /*prio*/,int device, u32 /*handle*/, u32 readId)
{
	STRVIS_PROCESS_REQUEST(device, readId);
}
#endif // STREAMING_VISUALIZE

void CStreamingDebug::InitDebugSystems()
{
#if STREAMING_VISUALIZE
	if (!pgStreamer::GetProcessCallback())
		pgStreamer::SetProcessCallback(s_DebugTracking);
#endif

#if STREAMING_VISUALIZE
	pgStreamer::SetBeginProcessCallback(s_DebugBeginTracking);
#endif
}

////////////////////////////////////////////////////////////

#if __BANK

enum
{
	REQ_FAILED_OUTOFMEM		= 1<<0,
	REQ_FAILED_FRAGMENTED	= 1<<1
};

struct strFailedRequest
{
	strIndex m_Index;
	float m_Timestamp;
	float m_LastSeen;
	u8 m_failReason;
	char m_AvailableMemory[192];
	atRangeArray<u8, 32> m_VirtualChunkSizes; // Number of chunks of each power of 2 size that were requested
	atRangeArray<u8, 32> m_PhysicalChunkSizes; // Number of chunks of each power of 2 size that were requested
	size_t m_TotalVirtual, m_TotalPhysical;
};

atArray<strFailedRequest> s_FailedRequests;
const float lingertime = 3000;

void CStreamingDebug::AddFailedStreamingAllocation(strIndex index, bool bFragmented, datResourceMap& map)
{
	if ( (bFragmented && !s_bTrackFailedReqFragmentation) || (!bFragmented && !s_bTrackFailedReqOOM) )
		return;

	const int count = s_FailedRequests.GetCount();
	int i = 0;
	for (; i < count; ++i)
	{
		if (s_FailedRequests[i].m_Index == index)
		{
			break;
		}
	}

	if (i == count)
	{
		s_FailedRequests.Grow().m_Timestamp = debugTimer.GetMsTime();
		s_FailedRequests[i].m_failReason = 0;
	}

	strFailedRequest& request = s_FailedRequests[i];
	request.m_Index = index;
	request.m_LastSeen = debugTimer.GetMsTime();
	request.m_failReason |= ( bFragmented ? REQ_FAILED_FRAGMENTED : REQ_FAILED_OUTOFMEM );

	sysMemSet(request.m_VirtualChunkSizes.GetElements(), 0x0, request.m_VirtualChunkSizes.GetMaxCount() * sizeof(request.m_VirtualChunkSizes[0]));

	request.m_TotalVirtual = 0;
	ASSERT_ONLY(int anyChunk = 0;)

	for(int i = 0; i < map.VirtualCount; i++)
	{
		datResourceChunk& chunk = map.Chunks[i];
		if (chunk.Size == 0)
		{
			continue;
		}
		request.m_TotalVirtual += chunk.Size;
		int leafUnits = (int)(((chunk.Size-1) / g_rscVirtualLeafSize) + 1); // - 1 and +1 to fix rounding issues. If leaf size is 0x100, all values from 0x301 to 0x400 should result in '4'
		int power = _CeilLog2(leafUnits);
		request.m_VirtualChunkSizes[power]++;
		ASSERT_ONLY(anyChunk++;)
	}

	sysMemSet(request.m_PhysicalChunkSizes.GetElements(), 0x0, request.m_PhysicalChunkSizes.GetMaxCount() * sizeof(request.m_PhysicalChunkSizes[0]));

	request.m_TotalPhysical = 0;
	for(int i = map.VirtualCount; i < map.VirtualCount + map.PhysicalCount; i++)
	{
		datResourceChunk& chunk = map.Chunks[i];
		if (chunk.Size == 0)
		{
			continue;
		}
		request.m_TotalPhysical += chunk.Size;
		int leafUnits = (int)(((chunk.Size-1) / g_rscPhysicalLeafSize) + 1);
		int power = _CeilLog2(leafUnits);
		request.m_PhysicalChunkSizes[power]++;
		ASSERT_ONLY(anyChunk++;)
	}

#if __ASSERT
	if (!anyChunk)
	{
		char infoString[256];
		Errorf("Bad datResourceMap:");

		for (int x=0; x<datResourceChunk::MAX_CHUNKS; x++)
		{
			datResourceChunk& chunk = map.Chunks[i];
			Errorf("Chunk %d: Src=%p, Dst=%p, Size=%" SIZETFMT "d", x, chunk.SrcAddr, chunk.DestAddr, chunk.Size);
		}

		map.Print(infoString, sizeof(infoString));
		Errorf("%s", infoString);

		// This can't really happen - we have an empty datResourceMap?
		Assertf(false, "Resource %s (%d) seems to have an empty resource map - that can't be right. It says %d virt chunks, %d phys chunks. See TTY for more info.",
			index.IsValid() ? strStreamingEngine::GetObjectName(index) : "(no resource)", index.Get(), map.VirtualCount, map.PhysicalCount);
	}
#endif // __ASSERT

	PrintCompactResourceHeapPages(request.m_AvailableMemory, sizeof(request.m_AvailableMemory));
}

void CStreamingDebug::UpdateFailedStreamingRequests()
{
	const int count = s_FailedRequests.GetCount();
	const float currenttime = debugTimer.GetMsTime();
	for (int i = count-1; i >= 0 ; --i)
	{
		if (currenttime - lingertime > s_FailedRequests[i].m_LastSeen)
		{
			s_FailedRequests.DeleteFast(i);
		}
	}
}

namespace {
	int FormatOneChunk(int num, size_t chunkSize, char* str, ptrdiff_t size)
	{
		const char* memory[4] = {"b", "Kb", "Mb", "Gb"};

		if (num == 0)
		{
			return 0;
		}

		int memoryUnit = 0;
		while(chunkSize >= 1024)
		{
			memoryUnit++;
			chunkSize >>= 10;
		}

		return formatf_n(str, size, "%dx%" SIZETFMT "d%s ", num, chunkSize, memory[memoryUnit]);
	}

	void FormatMemoryNeededString(strFailedRequest& request, char* str, ptrdiff_t size)
	{
		char* neededPtr = str;
		char* neededEnd = str + size;


		bool hasVirtual = false;
		for(int i = 0; i < request.m_VirtualChunkSizes.GetMaxCount(); i++)
		{
			if (request.m_VirtualChunkSizes[i] > 0)
			{
				hasVirtual = true;
				break;
			}
		}

		if (hasVirtual)
		{
			neededPtr += formatf_n(neededPtr, neededEnd - neededPtr, "M: ");
			for(int i = 0; i < request.m_VirtualChunkSizes.GetMaxCount(); i++)
			{
				int chunks = request.m_VirtualChunkSizes[i];
				size_t chunkSize = ((size_t)1 << (size_t)i) * g_rscVirtualLeafSize;
				neededPtr += FormatOneChunk(chunks, chunkSize, neededPtr, neededEnd - neededPtr);
			}
		}


		bool hasPhysical = false;
		for(int i = 0; i < request.m_PhysicalChunkSizes.GetMaxCount(); i++)
		{
			if (request.m_PhysicalChunkSizes[i] > 0)
			{
				hasPhysical = true;
				break;
			}
		}

		if (hasPhysical)
		{
			neededPtr += formatf_n(neededPtr, neededEnd - neededPtr, "V: ");
			for(int i = 0; i < request.m_PhysicalChunkSizes.GetMaxCount(); i++)
			{
				int chunks = request.m_PhysicalChunkSizes[i];
				size_t chunkSize = ((size_t)1 << (size_t)i) * g_rscPhysicalLeafSize;
				neededPtr += FormatOneChunk(chunks, chunkSize, neededPtr, neededEnd - neededPtr);
			}
		}

	}
}

void CStreamingDebug::DisplayFailedStreamingRequests()
{
	if (!s_bDisplayFailedRequests)
		return;

	Color32 aTextColors[] =
	{
		Color32(200, 0, 0, 255),		//out of memory
		Color32(255, 166, 0, 255)		//fragmentation
	};

	const int count = s_FailedRequests.GetCount();

	if (count)
	{
		DisplayResourceHeapPages();
		grcDebugDraw::AddDebugOutputEx(true, "** START FAILED REQUESTS - orange=too large to stream with fragmented mem, bug these**");
	}

	for (int i = 0; i < count; ++i)
	{
		strFailedRequest& request = s_FailedRequests[i];
		
		char memory[256] = {0};
		formatf(memory, "%8dK %8dK", request.m_TotalVirtual>>10, request.m_TotalPhysical>>10);
		char needed[256] = {0};

		FormatMemoryNeededString(request, needed, NELEM(needed));

		Color32& col = ( request.m_failReason & REQ_FAILED_FRAGMENTED ) ? aTextColors[1] : aTextColors[0] ;
		char buffer[255];
		formatf(buffer, "%s %s %s", strStreamingEngine::GetInfo().GetObjectName(request.m_Index), memory, needed);
		grcDebugDraw::AddDebugOutputEx(true, col, buffer);
		grcDebugDraw::AddDebugOutputEx(true, col, request.m_AvailableMemory);
	}

	if (count)
	{
		grcDebugDraw::AddDebugOutputEx(true, "** ENDS **");
	}
}

void CStreamingDebug::DisplayResourceHeapPages()
{
	sysMemDistribution distV;
	sysMemAllocator::GetCurrent().GetAllocator(MEMTYPE_RESOURCE_VIRTUAL)->GetMemoryDistribution(distV);
#if !ONE_STREAMING_HEAP
	sysMemDistribution distP;
	sysMemAllocator::GetCurrent().GetAllocator(MEMTYPE_RESOURCE_PHYSICAL)->GetMemoryDistribution(distP);
#endif // ONE_STREAMING_HEAP

	grcDebugDraw::AddDebugOutputEx(false, "Used/Free Virtual" 
#if !ONE_STREAMING_HEAP		
		", U/F Phys"
#endif // !ONE_STREAMING_HEAP
		);
	for (int s=12; s<32; s++) 
	{
#if !ONE_STREAMING_HEAP
		grcDebugDraw::AddDebugOutputEx(false, "%5dk: %5u/%-5u %5u/%-5u", 1<<(s-10),distV.UsedBySize[s],distV.FreeBySize[s],distP.UsedBySize[s],distP.FreeBySize[s]);
#else // ONE_STREAMING_HEAP
		grcDebugDraw::AddDebugOutputEx(false, "%5dk: %5u/%-5u", 1<<(s-10),distV.UsedBySize[s],distV.FreeBySize[s]);
#endif // !ONE_STREAMING_HEAP
	}
}

void CStreamingDebug::DisplayStolenMemory()
{
	atSimplePooledMapType<size_t, int> blockCounts;
	atFixedArray<size_t, 100> sizes;

	blockCounts.Init(100);


	// Create a map with all the block sizes
	for (int x=0; x<sAllocatedPageSizes.GetCount(); x++)
	{
		size_t blockSize = sAllocatedPageSizes[x];
		int oldCount = 0;
		int *oldCountPtr = blockCounts.Access(blockSize);

		if (oldCountPtr)
		{
			oldCount = *oldCountPtr;
		}
		else
		{
			sizes.Append() = blockSize;
		}

		blockCounts.m_Map[blockSize] = oldCount + 1;
	}

	// Sort by memory type, and then by block size.
	std::sort(sizes.begin(), sizes.end());
	int lastMemType = -1;
	char lineString[256];

	lineString[0] = 0;

	for (int x=0; x<sizes.GetCount(); x++)
	{
		size_t blockSize = sizes[x];

		// New type of memory?
		int memType = (blockSize & 0x80000000) ? 1 : 0;

		if (memType != lastMemType)
		{
			// Print the old one, if applicable.
			if (lineString[0])
			{
				grcDebugDraw::AddDebugOutputEx(false, lineString);
			}
			// Create a new header.
			formatf(lineString, "Stolen %c memory: ", (memType == 0) ? 'V' : 'M');
		}

		char substring[64];
		formatf(substring, "%dx%dKB ", blockCounts.m_Map[blockSize], (blockSize & ~0x80000000) / 1024);

		safecat(lineString, substring);
	}

	if (lineString[0])
	{
		grcDebugDraw::AddDebugOutputEx(false, lineString);
	}
}
#endif // __BANK

//////////////////////////////
// Streaming stats

#if __BANK
static bool s_bShowStreamingStats = false;
static bool s_bCollectStreamingStats = false;
static u32 s_numFrames = 0;
static u32 s_peakNumRequests;
static float s_avgNumRequests;
static u32 s_peakNumPrioRequests;
static float s_avgNumPrioRequests;

void StartCollectStreamingStats()
{
	s_bCollectStreamingStats = true;
	s_avgNumRequests = s_avgNumPrioRequests = 0.0f;
	s_peakNumRequests = s_peakNumPrioRequests = 0;
	s_numFrames = 0;
}

void StopCollectStreamingStats()
{
	s_bCollectStreamingStats = false;
}

void ShowStreamingStats()
{
	grcDebugDraw::AddDebugOutputEx(false, "requests = %2.3f [%d], prio = %2.3f [%d]",
		s_avgNumRequests,
		s_peakNumRequests,
		s_avgNumPrioRequests,
		s_peakNumPrioRequests);
}

void UpdateStreamingStats()
{
	if (s_bCollectStreamingStats)
	{
		u32 numRequests = strStreamingEngine::GetInfo().GetNumberRealObjectsRequested();
		s_peakNumRequests = Max(s_peakNumRequests, numRequests);
		s_avgNumRequests = (s_avgNumRequests * s_numFrames + numRequests) / (s_numFrames + 1);
		
		u32 numPrioRequests = numRequests * strStreamingEngine::GetIsLoadingPriorityObjects();
		s_peakNumPrioRequests = Max(s_peakNumPrioRequests, numPrioRequests);
		s_avgNumPrioRequests = (s_avgNumPrioRequests * s_numFrames + numPrioRequests) / (s_numFrames + 1);

		s_numFrames++;
	}
}

#endif // __BANK

#if __BANK
void PrintSkeletonData()
{
	// Skeletons
	Displayf("");

	Displayf("[CVehicle Skeletons]");
	CVehicle::PrintSkeletonData();

	Displayf("[CPed Skeletons]");
	CPed::PrintSkeletonData();

	Displayf("[CObject Skeletons]");
	CObject::PrintSkeletonData();

	Displayf("[CAnimatedBuilding Skeletons]");
	CAnimatedBuilding::PrintSkeletonData();
}

void PrintArchetypes()
{
	fwArchetypeManager::PoolFullCallback();
}

void PrintITypes()
{
	g_MapTypesStore.PrintStore();
}
#endif

#if __BANK
void TallyArchetypes()
{
	if (!s_EnableArchetypes)
		return;

	int totalSize = 0;
	fragCacheAllocator* pAllocator = fragManager::GetFragCacheAllocator();
	fwArchetypeMap& architectMap = fwArchetypeManager::GetArchetypeMap();

	for (int i = 0; i < (int) fwArchetypeManager::GetPool().GetSize(); ++i)
	{
		fwArchetype** ppArchetype = fwArchetypeManager::GetPool().GetSlot(i);	
		if (ppArchetype && *ppArchetype)
		{
			u32 key = (*ppArchetype)->GetHashKey();
			if (architectMap.Access(key) && pAllocator->IsValidUsedPointer(*ppArchetype))
				totalSize += (int) pAllocator->GetSizeWithOverhead(*ppArchetype);
		}
	}

	sprintf(s_TotalArchetypes, "%" SIZETFMT "d", fwArchetypeManager::GetPool().GetCount());
	sprintf(s_TotalArchetypeSize, "%d", totalSize);
}

void SaveArchetypes()
{
	fiStream* pFile = ASSET.Create(s_ArchetypePath, "");
	Assertf(pFile, "Unable to open: %s", s_ArchetypePath);

	if (pFile)
	{
		fprintf(pFile, "archetype,size,object\n");
		for(int i = 0; i < s_ArchetypeData.GetCount(); ++i)
		{
			ArchetypeDebugData& data = s_ArchetypeData[i];			
			fprintf(pFile, "%s,%d,0x%p\n", data.m_name.c_str(), data.m_size, data.m_object);
		}

		fflush(pFile);
		pFile->Close();
	}
}

void ClearArchetypes()
{
	for(int i = 0; i < s_ArchetypeData.GetCount(); ++i)
	{
		ArchetypeDebugData& data = s_ArchetypeData[i];
		u32 key = atHashValue::ComputeHash(data.m_name.c_str());
		s_ArchetypeList->RemoveItem(key);
	}

	s_ArchetypeData.clear();
}

void DisplayArchetypes()
{	
	ClearArchetypes();
	fragCacheAllocator* pAllocator = fragManager::GetFragCacheAllocator();
	fwArchetypeMap& architectMap = fwArchetypeManager::GetArchetypeMap();

	for (int i = 0; i < (int) fwArchetypeManager::GetPool().GetSize(); ++i)
	{
		fwArchetype** ppArchetype = fwArchetypeManager::GetPool().GetSlot(i);	
		if (ppArchetype && *ppArchetype)
		{										
			u32 key = (*ppArchetype)->GetHashKey();

			if (architectMap.Access(key) && pAllocator->IsValidUsedPointer(*ppArchetype))
			{
				atString name((*ppArchetype)->GetModelName());
				int usedMemory = (int) pAllocator->GetSizeWithOverhead(*ppArchetype);
				ArchetypeDebugData data = { name, usedMemory, *ppArchetype};
				s_ArchetypeData.Push(data);
			}
		}
	}

	int totalSize = 0;
	for(int i = 0; i < s_ArchetypeData.GetCount(); ++i)
	{		
		ArchetypeDebugData& data = s_ArchetypeData[i];
		totalSize += data.m_size;

		u32 key = atHashValue::ComputeHash(data.m_name.c_str());
		s_ArchetypeList->AddItem(key, 0, data.m_name.c_str());
		s_ArchetypeList->AddItem(key, 1, data.m_size);

		char address[16] = {0};
		sprintf(address, "0x%p", data.m_object);
		s_ArchetypeList->AddItem(key, 2, address);
	}

	sprintf(s_TotalArchetypes, "%d", s_ArchetypeData.GetCount());
	sprintf(s_TotalArchetypeSize, "%d", totalSize);
}

bool PopulateFragData(FragmentDebugData& data)
{
	if (data.m_type == rage::FragmentDebugData::FRAG_NONE)
	{
		for (int i = 0; i < CPed::GetPool()->GetSize(); ++i)
		{
			CPed* pPed = CPed::GetPool()->GetSlot(i);

			if (pPed && pPed->GetFragInst())
			{
				if (pPed->GetFragInst()->GetCacheEntry() == data.m_frag)
				{
					data.m_object = pPed;
					data.m_velocity = pPed->GetVelocity();
					data.m_type = (u32) rage::FragmentDebugData::FRAG_PED;
					data.m_reuse = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedIsInReusePool);
					break;
				}
			}
		}
	}

	if (data.m_type == rage::FragmentDebugData::FRAG_NONE)
	{
		for (int i = 0; i < CVehicle::GetPool()->GetSize(); ++i)
		{
			CVehicle* pVehicle = CVehicle::GetPool()->GetSlot(i);

			if (pVehicle && pVehicle->GetFragInst())
			{
				if (pVehicle->GetFragInst()->GetCacheEntry() == data.m_frag)
				{
					data.m_object = pVehicle;
					data.m_velocity = pVehicle->GetVelocity();
					data.m_type = (u32) rage::FragmentDebugData::FRAG_VEHICLE;
					data.m_reuse = pVehicle->GetIsInReusePool();
					break;
				}
			}
		}
	}	

	return (rage::FragmentDebugData::FRAG_NONE != data.m_type);
}

void TallyFragments()
{
	if (!s_EnableFragments)
		return;

	sprintf(s_TotalFragments, "%d", fragManager::GetFragCacheManager()->GetFramentCount());
	sprintf(s_TotalFragmentSize, "%d", fragManager::GetFragCacheManager()->GetFramentSize());
}

void SaveFragments()
{
	fiStream* pFile = ASSET.Create(s_FragmentPath, "");
	Assertf(pFile, "Unable to open: %s", s_FragmentPath);

	if (pFile)
	{
		fprintf(pFile, "fragment,size,type,reuse,object,frag,Velocity\n");

		for(int i = 0; i < s_FragmentData.GetCount(); ++i)
		{
			FragmentDebugData& data = s_FragmentData[i];

			const char* type;
			switch (data.m_type)
			{
			case rage::FragmentDebugData::FRAG_PED:
				type = "Ped";
				break;
			case rage::FragmentDebugData::FRAG_VEHICLE:
				type = "Vehicle";
				break;
			default:
				type = "None";
			}

			fprintf(pFile, "%s,%d,%s,%d,0x%p,0x%p,(%d %d %d)\n", data.m_name.c_str(), data.m_size, type, data.m_reuse, data.m_object, data.m_frag, data.m_velocity.GetX(), data.m_velocity.GetY(), data.m_velocity.GetZ());
		}

		fflush(pFile);
		pFile->Close();
	}
}

void ClearFragments()
{
	for(int i = 0; i < s_FragmentData.GetCount(); ++i)
	{
		FragmentDebugData& data = s_FragmentData[i];
		u32 key = atHashValue::ComputeHash(data.m_name.c_str());
		s_FragmentList->RemoveItem(key);
	}

	s_FragmentData.clear();
}

void DisplayFragments()
{
	// EJ: Pause the game loop to help avoid page faults
	fwTimer::StartUserPause(true);
	CStreaming::EnableSceneStreaming(false);

	ClearFragments();
	fragManager::GetFragCacheManager()->PopulateFragmentArray(s_FragmentData);	

	int totalSize = 0;
	for(int i = 0; i < s_FragmentData.GetCount(); ++i)
	{		
		FragmentDebugData& data = s_FragmentData[i];
		PopulateFragData(data);

		totalSize += data.m_size;

		u32 key = atHashValue::ComputeHash(data.m_name.c_str());
		s_FragmentList->AddItem(key, 0, data.m_name.c_str());
		s_FragmentList->AddItem(key, 1, data.m_size);

		switch (data.m_type)
		{
		case rage::FragmentDebugData::FRAG_PED:
			s_FragmentList->AddItem(key, 2, "Ped");
			break;
		case rage::FragmentDebugData::FRAG_VEHICLE:
			s_FragmentList->AddItem(key, 2, "Vehicle");
			break;
		default:
			s_FragmentList->AddItem(key, 2, "None");
		}
		
		s_FragmentList->AddItem(key, 3, (int) data.m_reuse);
		
		char buffer[16] = {0};
		sprintf(buffer, "0x%p", data.m_object);
		s_FragmentList->AddItem(key, 4, buffer);

		memset(buffer, 0, 16);
		sprintf(buffer, "0x%p", data.m_frag);
		s_FragmentList->AddItem(key, 5, buffer);

		memset(buffer, 0, 16);
		sprintf(buffer, "(%f,%f,%f)", data.m_velocity.GetX(), data.m_velocity.GetY(), data.m_velocity.GetZ());
		s_FragmentList->AddItem(key, 6, buffer);
	}

	sprintf(s_TotalFragments, "%d", s_FragmentData.GetCount());
	sprintf(s_TotalFragmentSize, "%d", totalSize);

	// EJ: Restore the game loop
	CStreaming::EnableSceneStreaming(true);
	fwTimer::EndUserPause();
}

void CreateArchFragWidgets(bkBank& bank)
{
	bank.PushGroup("Archetypes", false);
	bank.AddToggle("Enable Archetypes", &s_EnableArchetypes);
	bank.AddROText("Total Archetypes", s_TotalArchetypes, sizeof(s_TotalArchetypes));
	bank.AddROText("Total Archetype Size", s_TotalArchetypeSize, sizeof(s_TotalArchetypeSize));
	bank.AddText("Archetype File Path", s_ArchetypePath, sizeof(s_ArchetypePath));
	bank.AddButton("Save Archetypes", &SaveArchetypes);
	bank.AddButton("Clear Archetypes", &ClearArchetypes);
	bank.AddButton("Display Archetypes", &DisplayArchetypes);

	{
		sysMemAutoUseDebugMemory mem;
		s_ArchetypeData.Reserve(fwArchetypeManager::GetMaxArchetypeIndex());
	}
	
	s_ArchetypeList = bank.AddList("Archetypes");
	s_ArchetypeList->AddColumnHeader(0, "Name",	bkList::STRING);	
	s_ArchetypeList->AddColumnHeader(1, "Size", bkList::INT);
	s_ArchetypeList->AddColumnHeader(2, "Object", bkList::STRING);
	bank.PopGroup();

	// Fragments
	bank.PushGroup("Fragments", false);
	bank.AddToggle("Enable Fragments", &s_EnableFragments);
	bank.AddROText("Total Fragments", s_TotalFragments, sizeof(s_TotalFragments));
	bank.AddROText("Total Fragment Size", s_TotalFragmentSize, sizeof(s_TotalFragmentSize));
	bank.AddText("Fragment File Path", s_FragmentPath, sizeof(s_FragmentPath));
	bank.AddButton("Save Fragments", &SaveFragments);
	bank.AddButton("Clear Fragments", &ClearFragments);
	bank.AddButton("Display Fragments", &DisplayFragments);

	{
		sysMemAutoUseDebugMemory mem;
		s_FragmentData.Reserve(30000);
	}
	
	s_FragmentList = bank.AddList("Fragments");
	s_FragmentList->AddColumnHeader(0, "Name",	bkList::STRING);
	s_FragmentList->AddColumnHeader(1, "Size", bkList::INT);
	s_FragmentList->AddColumnHeader(2, "Type", bkList::STRING);	
	s_FragmentList->AddColumnHeader(3, "Reuse", bkList::INT);
	s_FragmentList->AddColumnHeader(4, "Object", bkList::STRING);
	s_FragmentList->AddColumnHeader(5, "Frag", bkList::STRING);
	s_FragmentList->AddColumnHeader(6, "Velocity", bkList::STRING);	
	bank.PopGroup();
}
#endif

//
// name:		CStreamingDebug::Update
// description:	Update function called every frame
void CStreamingDebug::Update()
{
#if __BANK
	TallyArchetypes();
	TallyFragments();	
#endif

#if COMMERCE_CONTAINER_DEBUG
	sysMemManager::GetInstance().GetVirtualMemHeap()->UpdateStats();
#endif

	IF_COMMERCE_STORE_RETURN();

	if (!strStreamingEngine::IsInitialised())
		return;

#if !__FINAL
	if ((!NetworkInterface::IsNetworkOpen() REPLAY_ONLY(&& !CReplayMgr::IsReplayInControlOfWorld())) || PARAM_enableflush.Get())
	{
		if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_F5, KEYBOARD_MODE_DEBUG, "flush scene"))
		{
            streamDebugf1("**********************************************************************");
            streamDebugf1("FLUSHING SCENE");
            streamDebugf1("**********************************************************************");
			CStreaming::GetCleanup().RequestFlush(false);
		}
		else if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_F5, KEYBOARD_MODE_DEBUG_SHIFT, "garbage collection only"))
		{
            streamDebugf1("**********************************************************************");
            streamDebugf1("FLUSHING SCENE (NON-MISSION)");
            streamDebugf1("**********************************************************************");
            CStreaming::GetCleanup().RequestFlushNonMission(false);
		}
	}

	if(BANK_ONLY(s_bCleanSceneEveryFrame || )CControlMgr::GetKeyboard().GetKeyJustDown(KEY_F5, KEYBOARD_MODE_DEBUG_SHIFT, "clean scene"))
	{
		if (!NetworkInterface::IsNetworkOpen())
		{
			CleanScene();
		}
	}
#endif

#if __BANK
	if (s_CycleDebugLocations)
	{
		// Do we need to switch locations?
		if (s_CurrentCycleLocation != s_NextAutomaticDebugLocation)
		{
			// Right before we teleport, we should collect stats.
			CollectMemoryProfileInformation(s_CurrentCycleLocation);

			// Let's do it.
			s_CurrentCycleLocation = s_NextAutomaticDebugLocation;

			// Skip meta locations.
			// Note that this code assumes that there is never a meta location at the end of the
			// list, which is currently always true (unless the list is empty).
			while (s_MemoryProfileLocations.m_Locations[s_CurrentCycleLocation].IsMetaLocation())
			{
				s_CurrentCycleLocation++;
				s_NextAutomaticDebugLocation++;
			}
			TeleportToMemoryProfileLocation(s_CurrentCycleLocation);

			// And reset the timer
			s_AutomaticDebugCycleTimer.Reset();
		}

		if ((u32) s_CurrentCycleLocation < (u32) s_MemoryProfileLocations.m_Locations.GetCount())
		{
			grcDebugDraw::AddDebugOutput("Automatic debug location cycling enabled.");
			grcDebugDraw::AddDebugOutput("Currently at %s - countdown to next: %.2f seconds",
				s_MemoryProfileLocations.m_Locations[s_CurrentCycleLocation].m_Name.c_str(),
				Max(s_AutomaticCycleSeconds - s_AutomaticDebugCycleTimer.GetTime(), 0.0f));
		}

		// Time for the next one?
		if (s_AutomaticDebugCycleTimer.GetTime() > s_AutomaticCycleSeconds)
		{
			// Switch to the next one.
			s_NextAutomaticDebugLocation++;

			if (s_NextAutomaticDebugLocation >= s_MemoryProfileLocations.m_Locations.GetCount())
			{
				// We're done. Reset the timer.
				CollectMemoryProfileInformation(s_CurrentCycleLocation);
				s_NextAutomaticDebugLocation = 0;
				s_CycleDebugLocations = false;
			}
		}
	}

	if (s_bShowDepsForSelectedEntity)
	{
		CEntity* pSelectedEntity = (CEntity*) g_PickerManager.GetSelectedEntity();
		if (pSelectedEntity)
		{
			// print streaming dependencies
			strIndex deps[STREAMING_MAX_DEPENDENCIES];
			fwModelId modelId = pSelectedEntity->GetModelId();
			strLocalIndex localIndex = CModelInfo::LookupLocalIndex(modelId);
			s32 numDeps = CModelInfo::GetStreamingModule()->GetDependencies(localIndex, &deps[0], STREAMING_MAX_DEPENDENCIES);
			u32 totalMain = 0;
			u32 totalVram = 0;
			for(s32 i=0; i<numDeps; i++)
			{
				if (deps[i].IsValid())
				{ 
					strStreamingInfo& info = *strStreamingEngine::GetInfo().GetStreamingInfo(deps[i]);
					strStreamingFile* pFile = strPackfileManager::GetImageFileFromHandle(info.GetHandle());
					u32 sizeVramKb = (info.ComputePhysicalSize(deps[i]) / 1024);
					u32 sizeMainKb = (info.ComputeOccupiedVirtualSize(deps[i]) / 1024);
					grcDebugDraw::AddDebugOutput("%s depends on %s (in %s) M:%dk, V:%dk",
						pSelectedEntity->GetModelName(),
						strStreamingEngine::GetObjectName(deps[i]),
						pFile->m_name.GetCStr(),
						sizeMainKb, sizeVramKb
						);

					totalMain += sizeMainKb;
					totalVram += sizeVramKb;
				}
			}
			grcDebugDraw::AddDebugOutput("%s total M:%dk, V:%dk", pSelectedEntity->GetModelName(), totalMain, totalVram);
		}
	}

	if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_DOWN, KEYBOARD_MODE_DEBUG, "move down in streaming lists"))
		gCurrentSelectedObj++;
	if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_UP, KEYBOARD_MODE_DEBUG, "move up in streaming lists") && gCurrentSelectedObj >= 0)
		gCurrentSelectedObj--;
	if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_RIGHT, KEYBOARD_MODE_DEBUG, "toggle dependencies in streaming lists"))
	{
		gbSeeObjDependencies = !gbSeeObjDependencies;
		gbSeeObjDependents = false;
	}
	if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_LEFT, KEYBOARD_MODE_DEBUG, "toggle dependents in streaming lists"))
	{
		gbSeeObjDependents = !gbSeeObjDependents;
		gbSeeObjDependencies = false;
	}

	if (s_bShowStreamingStats)
	{
		ShowStreamingStats();
	}
	UpdateStreamingStats();

	if (bDisplayResourceHeapPages)
	{
		DisplayResourceHeapPages();
	}

	if (sShowStolenMemory)
	{
		DisplayStolenMemory();
	}

	DisplayStressTestInfo();

#if USE_DEFRAGMENTATION
	if (s_bShowDefragStatus)
		strStreamingEngine::GetDefragmentation()->DisplayStatus();
#endif // USE_DEFRAGMENTATION

	if (s_ShowCollectedMemoryProfileStats)
		ShowCollectedMemoryProfileStats(true);

	if (bDisplayModuleMemory)
	{
		if (bDisplayModuleMemoryDetailed)
		{
			DisplayModuleMemoryDetailed(false, bDisplayModuleMemoryArtOnly, bDisplayModuleMemoryTempMemory);
		}
		else
		{
			DisplayModuleMemory(false, bDisplayModuleMemoryArtOnly, bDisplayModuleMemoryTempMemory);
		}
	}

#if STREAM_STATS
	if (s_ShowModuleStreamingStats)
	{
		ShowModuleStreamingStats();
	}
#endif // STREAM_STATS

#if __BANK
	if (bShowLoaderStatus)
	{
		strStreamingEngine::GetLoader().ShowFileStatus();
	}
	if (bShowAsyncStats)
	{
		strStreamingEngine::GetLoader().ShowAsyncStats();
	}
#endif // __BANK

#if TRACK_PLACE_STATS
	if (bDisplayModulePlaceStats)
	{
		strStreamingEngine::GetInfo().GetModuleMgr().DumpPlaceStats();
	}
#endif // TRACK_PLACE_STATS
#endif	//	__BANK

#if __BANK
	if (bDisplayRequestStats)
	{
		strStreamingEngine::GetInfo().DisplayRequestStats();

#if STREAMING_DETAILED_STATS
		CStreaming::GetCleanup().DisplayChurnStats();
#endif // STREAMING_DETAILED_STATS
	}
#endif // __BANK


#if __DEV
	for (int i = 0; i < 256; ++i)
	{
		if (s_entityList[i])
		{
			s_entityList[i]->SetIsVisibleForModule(SETISVISIBLE_MODULE_DEBUG, true);
		}
	}

#if !__WIN32PC && DEBUG_DRAW
	if (s_ShowLowLevelStreaming)
		ShowLowLevelStreaming();
#endif // !__WIN32PC && DEBUG_DRAW

	if (bDisplayStreamingOverview)
		strStreamingEngine::GetInfo().DisplayStreamingOverview(s_LastRequestLines, s_CompletedLines, s_RPFCacheMissLines, s_OOMLines, s_RemovedObjectLines);

#endif // __DEV
#if __BANK
	if (bDisplayStreamingInterests)
		g_SceneStreamerMgr.DumpStreamingInterests(false);
#endif // __BANK
#if __DEV

	if (bShowBucketInfoForSelected)
		ShowBucketInfoForSelected();

	if (bDisplayChurnPoint)
		CStreaming::GetCleanup().DisplayChurnPoint();

	if(bDisplayRequestsByModule)
		DisplayRequestedObjectsByModule();
	if(bDisplayTotalLoadedObjectsByModule)
		DisplayTotalLoadedObjectsByModule();

#endif // __DEV
#if __BANK
	if (s_ShowModuleInfoOnScreen)
		DisplayObjectsInMemory(NULL, true, s_moduleId - 1, s_DumpFilter);
#endif // __BANK
#if __DEV

#if __HIERARCHICAL_NODES_ENABLED
	if(bDisplayHierarchicalNavNodesInMemory)
		DisplayObjectsInMemory(NULL, true, CPathServer::GetHierarchicalNodesStreamingModuleId());
#endif

#endif	//	__DEV

#if __SCRIPT_MEM_CALC && !__GAMETOOL
	CTheScripts::GetScriptHandlerMgr().CalculateMemoryUsage();
#endif // __SCRIPT_MEM_CALC && !__GAMETOOL

//	if (bDisplayModelsUsedbyScripts)
// 	{
// 		if (CScriptDebug::GetDisplayModelUsagePerScript())
// 			CTheScripts::GetScriptHandlerMgr().DisplayStreamingMemoryUsage();
// 		else
// 			CTheScripts::GetScriptHandlerMgr().DisplayMemoryUsage();
// 	}

#if __DEV
	if(bDisplayPedsInMemory)
		DisplayModelsInMemory(CModelInfo::GetPedModelInfoStore());
	if(bDisplayVehiclesInMemory)
		DisplayModelsInMemory(CModelInfo::GetVehicleModelInfoStore());
	if(bDisplayWeaponsInMemory)
		DisplayModelsInMemory(CModelInfo::GetWeaponModelInfoStore());
#endif // __DEV

#if __BANK
	if(bDisplayMemoryUsage)
		DisplayMemoryUsage();
	if(bDisplayBucketUsage)
		DisplayMemoryBucketUsage();
	if(bDisplayPoolsUsage)
		DisplayPoolsUsage();
	if(bDisplayStoreUsage)
		DisplayStoreUsage();
	if(bDisplayPackfileUsage)
		DisplayPackfileUsage();
#if __DEV
	if(bDisplayMatrixUsage)
		DisplayMatrixUsage();
#endif

#if USE_RESOURCE_CACHE
	grcResourceCache::GetInstance().DisplayStats();
#endif

#if LIVE_STREAMING
	strStreamingEngine::GetLive().DrawDebugInfo(grcDebugDraw::AddDebugOutput);
#endif // LIVE_STREAMING

	fwTransform::DisplayDebugInfo();

	DEBUGSTREAMGRAPH.Update();

	/*
	if(CGameWorld::GetMainPlayerInfo() && CGameWorld::GetMainPlayerInfo()->GetPlayerPed())
	{
		CPed* pPlayerPed = CGameWorld::GetMainPlayerInfo()->GetPlayerPed();
		const Vector3 vPlayerPos = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());
	}
	*/
#endif // __BANK

#if __BANK
	if (!CStreaming::IsStreamingPaused() && !s_bPauseFailedRequestList)
		UpdateFailedStreamingRequests();

	DisplayFailedStreamingRequests();
#endif
}

//////////////////////////////
// Widgets

#if __BANK

//
// name:		CStreamingDebug::InitWidgets
// description:	Initialise streaming debug code
void CStreamingDebug::InitWidgets()
{
	// Create the weapons bank
	bkBank* pBank = &BANKMGR.CreateBank("Streaming & Memory", 0, 0, false); 
	if(streamVerifyf(pBank, "Failed to create streaming & memory bank"))
	{
		pBank->AddButton("Create streaming memory widgets", datCallback(CFA1(CStreamingDebug::AddWidgets), pBank));
	}

#if LIVE_STREAMING
	strStreamingEngine::GetLive().AddBankWidgets();
#endif

	strPackfileManager::GetStreamingModule()->SetExpectedVirtualSize(356);
	g_DwdStore.SetExpectedVirtualSize(14080);				g_DwdStore.SetExpectedPhysicalSize(19226);
	g_DrawableStore.SetExpectedVirtualSize(7828);			g_DrawableStore.SetExpectedPhysicalSize(22956);
	g_TxdStore.SetExpectedVirtualSize(1328);				g_TxdStore.SetExpectedPhysicalSize(73390);
	g_FragmentStore.SetExpectedVirtualSize(5904);			g_FragmentStore.SetExpectedPhysicalSize(2881);
	g_ClipDictionaryStore.SetExpectedVirtualSize(19636);		
	g_ExpressionDictionaryStore.SetExpectedVirtualSize(4136);
	g_StaticBoundsStore.SetExpectedVirtualSize(15248);
	g_StreamedScripts.SetExpectedVirtualSize(1088);
	ptfxManager::GetAssetStore().SetExpectedVirtualSize(4316);	ptfxManager::GetAssetStore().SetExpectedPhysicalSize(4622);
	s32 moduleId = ThePaths.GetStreamingModuleId();
	strStreamingModule* module = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(moduleId);
	module->SetExpectedVirtualSize(768);
	moduleId = CPathServer::GetNavMeshStreamingModuleId(kNavDomainRegular);
	if(moduleId != -1)
	{
		module = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(moduleId);
		module->SetExpectedVirtualSize(5172);
	}

	// EJ: Auto-toggle memory usage display
	if (PARAM_MemoryUsage.Get())
	{
		bDisplayMemoryUsage = true;
	}

	CIslandHopper::GetInstance().InitWidgets(*pBank);
}

// EJ: Test
#if COMMERCE_CONTAINER_DEBUG
void LoadModule()
{
	CLiveManager::GetCommerceMgr().ContainerLoadModule();
}

void UnloadModule()
{
	CLiveManager::GetCommerceMgr().ContainerUnloadModule();
}

void LoadStore()
{
	CLiveManager::GetCommerceMgr().ContainerLoadStore();
}

void UnloadStore()
{
	CLiveManager::GetCommerceMgr().ContainerUnloadStore();
}

void LoadFacebookPRX()
{
	CLiveManager::GetCommerceMgr().ContainerLoadGeneric(CELL_SYSMODULE_SYSUTIL_NP_SNS, "Facebook");
}

void UnloadFacebookPRX()
{
	CLiveManager::GetCommerceMgr().ContainerUnloadGeneric(CELL_SYSMODULE_SYSUTIL_NP_SNS, "Facebook");
}

void LoadAudioPRX()
{
	CLiveManager::GetCommerceMgr().ContainerLoadGeneric(CELL_SYSMODULE_AVCONF_EXT, "Audio Pulse Wireless Detection");
}

void UnloadAudioPRX()
{
	CLiveManager::GetCommerceMgr().ContainerUnloadGeneric(CELL_SYSMODULE_AVCONF_EXT, "Audio Pulse Wireless Detection");
}

#if __STATS
void OnVirtualMemStats()
{
	VirtualMemoryStats::UpdateVirtualMemoryProfileStats(sysMemManager::GetInstance().GetVirtualMemHeap());

	// Header
	streamDisplayf(" ----------------- PS3 Virtual Mem Stats ----------------- ");
	
	pfPage *page = GetRageProfiler().GetPageByName("Virtual Memory Stats");
	if (page)
	{
		pfGroup * group = NULL;
		group = page->GetNextGroup(group);
		while(group)
		{
			if(group->GetActive())
			{
				pfValue* value = NULL;
				value = group->GetNextValue(value);
				while (value)
				{
					s32 index = -1;
					if(value->GetActive())
						streamDisplayf("%20s %20s %08f", page->GetName(), value->GetName(index), value->GetAsFloat());

					value = group->GetNextValue(value);
				}
			}
			group = page->GetNextGroup(group);
		}
	}

	VirtualMemoryStats::VirtualMemoryProfileEndFrame();
}
#endif
#endif // COMMERCE_CONTAINER_DEBUG

void ReportSkeletonMetrics()
{
#if !__FINAL
	CNetworkDebugTelemetry::AppendMetricSkeleton(CDebug::GetCurrentMissionName());
#endif
}

#if RAGE_POOL_TRACKING
void ReportPoolMetrics()
{
	CNetworkDebugTelemetry::AppendMetricPoolUsage(CDebug::GetCurrentMissionName(), true);
}
#endif

#if ENABLE_STORETRACKING
void ReportUsageMetrics()
{
	CNetworkDebugTelemetry::AppendMetricMemUsage(CDebug::GetCurrentMissionName(), true);
}

void ReportStoreMetrics()
{
	CNetworkDebugTelemetry::AppendMetricMemStore(CDebug::GetCurrentMissionName(), true);
}
#endif

#if MEMORY_TRACKER
void PrintXenonBuckets()
{
	sysMemManager::GetInstance().GetSystemTracker()->Print();
}
#endif

void OnToggleArea51()
{
	Assertf(strStreamingEngine::GetInfo().GetNumberObjectsRequested(), "Don't toggle Area 51 with pending requests");
}

#if RSG_ORBIS
void PrintSystemMemory()
{
	/* https://ps4.scedev.net/forums/thread/7291/
	   https://ps4.scedev.net/forums/thread/21183/
	   You can use sceKernelVirtualQuery to get information about all mapped memory. You have to call it for each area, but if you 
	   call it with SCE_KERNEL_VQ_FIND_NEXT specified then you can just pass the returned info->end address in to the next call until 
	   it returns SCE_KERNEL_ERROR_EACCES to iterate over every block.
	 */
	int32_t result = 0;
	void* pAddress = NULL;
	SceKernelVirtualQueryInfo info;

	while ((result = sceKernelVirtualQuery(pAddress, SCE_KERNEL_VQ_FIND_NEXT, &info, sizeof(SceKernelVirtualQueryInfo))) != SCE_KERNEL_ERROR_EACCES)
	{
		pAddress = info.end;
		const size_t size = reinterpret_cast<size_t>(info.end) - reinterpret_cast<size_t>(info.start);
		Memoryf("%s,%" SIZETFMT "u\n", (info.name && strlen(info.name) > 0) ? info.name : "<unknown>", size);		
	}
}
#endif

#if __DEV
bool bSanityCheckOnAlloc = false;
void SanityCheckOnAllocCB(void)
{

	sysMemSimpleAllocator *gameHeap = dynamic_cast<sysMemSimpleAllocator *>(sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_GAME_VIRTUAL));
	if(gameHeap)
	{
		gameHeap->SanityCheckHeapOnDoAllocate(bSanityCheckOnAlloc);
	}
}
#endif
void CStreamingDebug::AddWidgets(bkBank& bank)
{
	// destroy first widget which is the create button
	bkWidget* pWidget = BANKMGR.FindWidget("Streaming & Memory/Create streaming memory widgets");
	if(pWidget == NULL)
		return;
	pWidget->Destroy();

	s_StreamerThreadPriority = CGameConfig::Get().GetThreadPriority(ATSTRINGHASH("streamer", 0x13366CE2), (__PS3) ? PRIO_HIGHEST:PRIO_NORMAL);

	// ----- Part 0: Archetypes and Fragments
	bank.PushGroup("Archetypes & Fragments", false);
	CreateArchFragWidgets(bank);
	bank.PopGroup();

	// ----- Part 1: MEMORY -----------------
	bank.PushGroup("Memory", false);

	bank.PushGroup("Memory Usage");

	bank.AddToggle("Memory usage", &bDisplayMemoryUsage);

#if RSG_ORBIS
	bank.AddButton("Print System Memory", &PrintSystemMemory);
#endif

#if (__PPU || __XENON) && !__TOOL && !__RESOURCECOMPILER
	bank.AddButton("PrintMemoryUsage", &sysMemSimpleAllocator::PrintAllMemoryUsage);
#endif

#if ENABLE_STORETRACKING
	bank.AddButton("Report Usage Metrics", &ReportUsageMetrics);
	bank.AddButton("Report Store Metrics", &ReportStoreMetrics);
#endif

#if RAGE_POOL_TRACKING
	bank.AddButton("Report Pool Metrics", &ReportPoolMetrics);
#endif

	//bank.AddButton("Report Skeleton Metrics", &ReportSkeletonMetrics);
	bank.AddButton("Print Skeleton Memory", &PrintSkeletonData);
	bank.AddButton("Print Archetypes", &PrintArchetypes);
	bank.AddButton("Print I-Types", &PrintITypes);

#if MEMORY_TRACKER
	bank.AddButton("Print Xenon Buckets", &PrintXenonBuckets);
#endif

	bank.PushGroup("Automated Memory Metrics");
	if (GetMemoryProfileLocationNames() == NULL)
	{
		LoadMemoryProfileLocations();
	}

	bank.AddCombo("Debug Location", &s_MemoryProfileLocationTarget, s_MemoryProfileLocationCount, s_MemoryProfileLocationNames);
	bank.AddButton("Compare current location against selected", datCallback(CompareAgainstSelected));
	bank.AddButton("Teleport to Debug Location", datCallback(TeleportToMemoryProfileLocation));
	bank.AddToggle("Cycle through all debug locations", &s_CycleDebugLocations);
	bank.AddSlider("Number of seconds to stay at each location", &s_AutomaticCycleSeconds, 0.1f, 60.0f, 0.1f);
	bank.AddButton("Reset automatic cycle", datCallback(ResetAutomaticDebugLocationCycle));
	bank.AddButton("Collect memory stats here", datCallback(CStreamingDebug::CollectMemoryProfileInformationFromCurrent));
	bank.AddToggle("Show collected stats", &s_ShowCollectedMemoryProfileStats);
	bank.AddButton("Dump collected stats", datCallback(DumpCollectedMemoryProfileStats));
	bank.AddButton("Save location stats", datCallback(SaveMemoryProfileLocations));
	bank.AddButton("Dump modules compared to average", datCallback(CStreamingDebug::DumpModuleMemoryComparedToAverage));
	bank.PopGroup(); // Automated Memory Metrics

	bank.PushGroup("Per-Module Info");
	bank.PushGroup("Summaries");
	bank.AddToggle("Display modules", &bDisplayModuleMemory);
	bank.AddToggle("Show scene detail", &bDisplayModuleMemoryDetailed);
	bank.AddToggle("Show art only", &bDisplayModuleMemoryArtOnly);
	bank.AddToggle("Show temporary memory", &bDisplayModuleMemoryTempMemory);
	bank.AddButton("Dump Module Memory", datCallback(DumpModuleMemory));	
	bank.PopGroup();

#if __DEV
	bank.PushGroup("LA Noire Style");
	bank.AddToggle("Streaming Requests By Module", &bDisplayRequestsByModule);
	bank.AddToggle("Total Loaded objects by module", &bDisplayTotalLoadedObjectsByModule);
	bank.AddToggle("Hide dummy requests on HDD", &bHideDummyRequestsOnHdd);
	bank.PopGroup();
#endif // __DEV

	bank.PushGroup("Master Module Display");

	s_moduleNames.Reserve(strStreamingEngine::GetInfo().GetModuleMgr().GetNumberOfModules()+1);
	s_moduleNames.Append() = "";
	for (int i = 0; i < strStreamingEngine::GetInfo().GetModuleMgr().GetNumberOfModules();++i)
	{
		s_moduleNames.Append() = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(i)->GetModuleName();
	}

	static const char* sortType[] = {"none","name","size","virtual size","physical size","chunks","num refs", "lod distance", "flags", "img", "extra info", "LSN"};
	bank.AddToggle("Show module info on screen", &s_ShowModuleInfoOnScreen);
	bank.AddCombo("Show module objects", (int*)&s_moduleId,s_moduleNames.GetCount(),s_moduleNames.GetElements());
	bank.AddCombo("Sort objects by", (int*)&s_eSortDisplayedObjects,sizeof(sortType)/sizeof(char*),sortType);
	bank.AddToggle("Filter by size", &bDisplayOnlyWithinMemRange);
	bank.AddText("Resource Search", s_ModuleSubstringSearch, sizeof(s_ModuleSubstringSearch));
	bank.AddSlider("MinKb", &minMemSizeToDisplayKb, 0, 512*1024, 1);
	bank.AddSlider("MaxKb", &maxMemSizeToDisplayKb, 0, 512*1024, 1);

	bank.AddToggle("Unloaded Files", &s_DumpFilter, 1 << STRINFO_NOTLOADED);
	bank.AddToggle("Requested Files", &s_DumpFilter, 1 << STRINFO_LOADREQUESTED);
	bank.AddToggle("Loading Files", &s_DumpFilter, 1 << STRINFO_LOADING);
	bank.AddToggle("Loaded Files", &s_DumpFilter, 1 << STRINFO_LOADED);
	bank.AddToggle("Include Dummies", &s_DumpIncludeDummies);
	bank.AddToggle("Include Dependencies", &s_DumpIncludeDependencies);
	bank.AddToggle("Include Temp Memory", &s_DumpIncludeTempMemory);
	bank.AddToggle("Exclude Scene Assets", &s_DumpExcludeSceneAssets);
	bank.AddToggle("Only Scene Assets", &s_DumpOnlySceneAssets);
	bank.AddToggle("Exclude Scene Assets For Rendering Only", &s_ExcludeSceneAssetsForRendering);
	bank.AddToggle("Exclude Scene Assets Not For Rendering Only", &s_ExcludeSceneAssetsForNonRendering);
	bank.AddToggle("Required Only", &s_DumpRequiredOnly);
	bank.AddToggle("Zoned Assets Only", &s_DumpOnlyZone);
	bank.AddToggle("Add Dependency Cost", &s_DumpAddDependencyCost);
	bank.AddToggle("Immediate Dependency Cost Only", &s_DumpImmediateDependencyCostOnly);
	bank.AddToggle("Show Entities", &s_DumpShowEntities);
	bank.AddToggle("Include Inactive Entities", &s_DumpIncludeInactiveEntities);
	bank.AddToggle("Show Cached Only", &s_DumpCachedOnly);
	bank.AddToggle("Show With No Entities Only", &s_DumpOnlyNoEntities);
	bank.AddToggle("Split By Device", &s_DumpSplitByDevice);
	bank.AddText("File to dump to (none=TTY)", s_DumpOutputFile, sizeof(s_DumpOutputFile));
	bank.AddButton("Dump Module Info", datCallback(DumpModuleInfo));
	bank.AddButton("Dump Module Info (all objects in game)", datCallback(DumpModuleInfoAllObjects));
	bank.AddToggle("Show Dependencies", &bShowDependencies);
	bank.AddToggle("Unfulfilled Dependencies Only", &bShowUnfulfilledDependencies);
	bank.AddToggle("Bounding Box Around Selected Entities", &bDrawBoundingBoxOnEntities);
	bank.AddToggle("Select, don't Frame Entities", &bSelectDontFrameEntities);
	bank.PopGroup(); // Master Module Display
	bank.PopGroup(); // Per-module info

	bank.PushGroup("Per-Resource Info");

	bank.PushGroup("Stores");
	g_StaticBoundsStore.InitWidgets(bank);

#if __DEV
	bank.AddToggle("Store usage", &bDisplayStoreUsage);
	bank.AddToggle("Packfile usage", &bDisplayPackfileUsage);
#endif // __DEV
	bank.PopGroup();

#if __DEV
	bank.PushGroup("Model types", false);
	bank.AddToggle("Vehicles", &bDisplayVehiclesInMemory);
	bank.AddToggle("Peds", &bDisplayPedsInMemory);
	bank.AddToggle("Weapons", &bDisplayWeaponsInMemory);
	bank.AddToggle("Trees", &bDisplayTreesInMemory);
	bank.PopGroup();
#endif // __DEV

	ResourceMemoryUseDump::AddWidgets(bank);
#if __DEV
	bank.AddSlider("Large resource (MB)", &s_largeResourceSize, 0, 25, 1, NullCB);
	bank.AddButton("Write large resources", datCallback(WriteLargeResources));

	bank.AddButton("Write vehicle object sizes", WriteVehicleModelSizes);
	bank.AddButton("Write ped object sizes", WritePedModelSizes);
#endif // __DEV

	bank.AddToggle("Show dependencies for selected entity", &s_bShowDepsForSelectedEntity);

	bank.PopGroup();	// Per-resource info

#if COMMERCE_CONTAINER_DEBUG
	bank.AddButton("Save Module", &LoadModule);
	bank.AddButton("Load Module", &UnloadModule);
	bank.AddButton("Save Store", &LoadStore);
	bank.AddButton("Load Store", &UnloadStore);
	bank.AddButton("Save Facebook", &LoadFacebookPRX);
	bank.AddButton("Load Facebook", &UnloadFacebookPRX);
	bank.AddButton("Save Audio Pulse Wireless Detection", &LoadAudioPRX);
	bank.AddButton("Load Audio Pulse Wireless Detection", &UnloadAudioPRX);
#if __STATS
	bank.AddButton("Virtual Mem Stats", &OnVirtualMemStats);
#endif
#endif // COMMERCE_CONTAINER_DEBUG

	bank.AddToggle("Memory bucket usage", &bDisplayBucketUsage);
	bank.AddToggle("Resource Heap Pages", &bDisplayResourceHeapPages);
	bank.AddToggle("Pools usage", &bDisplayPoolsUsage);
	bank.AddToggle("Sort Pool usage by size", &bSortBySize);
	bank.AddText("Filter pool usage", s_achFilterPoolName, RAGE_MAX_PATH);
	bank.AddButton("Dump pools usage to tty", datCallback(PrintPoolUsage));
	bank.AddButton("Dump stores usage to tty", datCallback(PrintStoreUsage));

#if __DEV
	bank.AddToggle("Matrix usage", &bDisplayMatrixUsage);
	fwTransform::SetupWidgets(bank);
#endif // __DEV

	DEBUGSTREAMGRAPH.AddWidgets(&bank);

#if __SCRIPT_MEM_DISPLAY
	bank.PushGroup("Script Memory");
	bank.AddToggle("Script Memory", &CScriptDebug::GetDisplayScriptMemoryUsage());
	bank.AddToggle("Detailed Script Memory", &CScriptDebug::GetDisplayDetailedScriptMemoryUsage());
	bank.AddToggle("Filter Detailed Script Memory Display", &CScriptDebug::GetDisplayDetailedScriptMemoryUsageFiltered());
	bank.AddToggle("Exclude Scripts from Script Memory Display", &CScriptDebug::GetExcludeScriptFromScriptMemoryUsage());
	bank.PopGroup();
#endif	//	__SCRIPT_MEM_DISPLAY

#if __PPU
	bank.AddButton("Write memory report", datCallback(WriteMemoryReport));
#endif

	bank.AddButton("Full Memory Dump", datCallback(CStreamingDebug::FullMemoryDump));

	bank.PopGroup();	// Memory usage

	bank.PushGroup("Steal memory");
	bank.AddSlider("Resource memory page to allocate", &sPageSizeToAllocate, 12, 24, 1, datCallback(UpdatePageSizeStringToAllocate));
	bank.AddText("Size of resource page to allocate", sPageSizeToAllocateStr, sizeof(sPageSizeToAllocateStr), true);
	bank.AddToggle("Resource memory to allocate is virtual", &sPageToAllocateIsVirtual, datCallback(UpdatePageSizeStringToAllocate));
	bank.AddButton("Allocate resource memory page", datCallback(AllocateResourceMemoryPage));
	bank.AddButton("Allocate pages to fragment memory", datCallback(AllocateBlocksToFragmentMemory));
	bank.AddButton("Free all stolen pages", datCallback(FreeAllAllocatedResourceMemoryPages));
	bank.AddToggle("Show stolen memory", &sShowStolenMemory);
	bank.PopGroup();	// Steal memory

	if(PARAM_emergencystop.Get())
	{
		bank.AddToggle("Suspend Stop On Streaming Emergency", &g_SuspendEmergencyStop);
	}

#if USE_DEFRAGMENTATION
	bank.PushGroup("Defragmentation");
#if __BANK
	bank.AddToggle("Defrag Enable", &strStreamingEngine::GetDefragmentation()->GetEnabled(), NullCB);
	bank.AddButton("Abort defrag", datCallback(AbortDefrag));
#endif // __DEV

	bank.AddToggle("Show defrag status", &s_bShowDefragStatus);
	bank.AddToggle("Defragorama!", strStreamingEngine::GetDefragmentation()->GetDefragoramaBoolPtr());
	bank.AddToggle("Don't update defrag when paused", strStreamingEngine::GetDefragmentation()->GetDontUpdateWhenPausedPtr());
	bank.PopGroup();	// Defragmentation
#endif // USE_DEFRAGMENTATION

	bank.AddSeparator();

	static const char *s_HeapNames[] = { "GameVirt", "RscVirt", "GamePhys", "RscPhys", "Debug"
#if __XENON
		, "XMemAlloc"
#endif // __XENON
	};

	if (s_MemoryStressTesters.GetCount() == 0)
	{
		USE_DEBUG_MEMORY();
		s_MemoryStressTesters.Reserve(8);

		s_MemoryStressTestSetup.m_MinAlloc = 4096;
		s_MemoryStressTestSetup.m_MaxAlloc = 32768;
		s_MemoryStressTestSetup.m_CpuId = 1;
		s_MemoryStressTestSetup.m_MinWait = 1;
		s_MemoryStressTestSetup.m_MaxWait = 100;
		s_MemoryStressTestSetup.m_MaxAllocCount = 4;
		s_MemoryStressTestSetup.m_Prio = PRIO_NORMAL;
	}

	bank.PushGroup("Stress Test");
	bank.AddSlider("Min Allocation Size", &s_MemoryStressTestSetup.m_MinAlloc, 1, 8 * 1024 * 1024, 1);
	bank.AddSlider("Max Allocation Size", &s_MemoryStressTestSetup.m_MaxAlloc, 1, 8 * 1024 * 1024, 1);
	bank.AddCombo("Heap To Use", &s_MemoryStressTestSetup.m_Heap, NELEM(s_HeapNames), s_HeapNames);
	bank.AddSlider("Min Wait (ms)", &s_MemoryStressTestSetup.m_MinWait, 0, 10000, 1);
	bank.AddSlider("Max Wait (ms)", &s_MemoryStressTestSetup.m_MaxWait, 0, 10000, 1);
	bank.AddSlider("CPU ID", &s_MemoryStressTestSetup.m_CpuId, 0, 5, 1);
	bank.AddSlider("Thread priority", &s_MemoryStressTestSetup.m_Prio, PRIO_LOWEST, PRIO_HIGHEST, 1);
	bank.AddSlider("Max Alloc Count", &s_MemoryStressTestSetup.m_MaxAllocCount, 1, MemoryStressTester::MAX_ALLOCS, 1);
	bank.AddButton("CREATE THREAD", datCallback(CreateMemoryStressTestThread));
	bank.AddToggle("Delete all stress threads", (bool *) &s_DeleteAllStressTesters);

	bank.PopGroup();

#if __DEV
	bank.AddButton("Sanity check memory heaps", &SanityCheckMemoryHeaps);
	bank.AddToggle("Sanity check on alloc", &bSanityCheckOnAlloc, SanityCheckOnAllocCB);
#endif //__DEV

	bank.PopGroup();	//	Memory

	// ------- Part 2: Streaming ----------------

	bank.PushGroup("Streaming", false);
	bank.AddToggle("Crash The Streamer :)", &pgStreamer::force_streamer_crash);

	bank.AddToggle("** AREA 51 **", &g_AREA51, OnToggleArea51);

	bank.PushGroup("Logging");

#if STREAMING_VISUALIZE
	if (strStreamingVisualize::IsInstantiated())
	{
		STREAMINGVIS.AddWidgets(bank);
	}
	
#endif // STREAMING_VISUALIZE
#if PGSTREAMER_DEBUG
	if(PARAM_streamingekg.Get())
	{
		StrTicker::AddWidgets(bank);
	}
#endif


#if STREAMING_TELEMETRY_INTERFACE
	bank.PushGroup("Telemetry");
	bank.AddToggle("Telemetry Log", strStreamingEngine::GetEnableTelemetryLogPtr());
	bank.PopGroup();
#endif // STREAMING_TELEMETRY_INTERFACE

#if __DEV
	bank.PushGroup("Streaming Log", false);
	bank.AddText("Streaming log", s_LogStreamingName, 256, datCallback(ChangeStreamingLog));
	bank.AddButton("Pick file", datCallback(SelectStreamingLog));
	bank.AddToggle("Enable log", &s_bLogStreaming, datCallback(ToggleStreamingLog));
	bank.AddToggle("Append to log", &s_bLogAppend);
	bank.PopGroup();
#endif //__DEV

	bank.PushGroup("Streaming Overview");
#if !__WIN32PC && DEBUG_DRAW
	bank.AddToggle("Low-Level Streaming", &s_ShowLowLevelStreaming);
#endif // !__WIN32PC && DEBUG_DRAW
#if __DEV
	bank.AddToggle("Streaming Overview", &bDisplayStreamingOverview);
#endif // __DEV
	bank.AddToggle("Show Request Stats", &bDisplayRequestStats);
	bank.AddButton("Reset Request Stats", datCallback(ResetStreamingStats));
#if __DEV
	bank.AddSlider("Last Request Lines", &s_LastRequestLines, 0, strStreamingInfoManager::REQUEST_LOG_SIZE, 1);
	bank.AddSlider("Completed Lines", &s_CompletedLines, 0, strStreamingInfoManager::REQUEST_LOG_SIZE, 1);
	bank.AddSlider("RPF Cache Miss Lines", &s_RPFCacheMissLines, 0, strStreamingInfoManager::REQUEST_LOG_SIZE, 1);
	bank.AddSlider("Out Of Memory Lines", &s_OOMLines, 0, strStreamingInfoManager::REQUEST_LOG_SIZE, 1);
	bank.AddSlider("Removed Objects Lines", &s_RemovedObjectLines, 0, strStreamingInfoManager::REQUEST_LOG_SIZE, 1);
	bank.AddToggle("Streaming Log To TTY", strStreamingEngine::GetInfo().GetLogActivityToTTYPtr());
	bank.AddButton("Print Request List (tty)", datCallback(PrintRequestList));
	bank.AddButton("Print Loaded List, required only (tty)", datCallback(PrintLoadedListRequiredOnly));
	bank.AddButton("Print Loaded List (tty)", datCallback(PrintLoadedList));
	bank.AddButton("Print Loaded List as CSV (tty)", datCallback(PrintLoadedListAsCSV));
	bank.AddToggle("Print dependencies as well", &s_printDependencies);
	bank.PopGroup();

	bank.PushGroup("Failed requests");
	bank.AddToggle("Track failed because OOM", &s_bTrackFailedReqOOM);
	bank.AddToggle("Track failed because fragmented", &s_bTrackFailedReqFragmentation);
	bank.AddToggle("Display failed requests", &s_bDisplayFailedRequests);
	bank.AddToggle("Pause update of failed requests", &s_bPauseFailedRequestList);
	bank.PopGroup();

	bank.PushGroup("Streaming stats");
	bank.AddToggle("Show stats", &s_bShowStreamingStats);
	bank.AddButton("Start", datCallback(StartCollectStreamingStats));
	bank.AddButton("End", datCallback(StopCollectStreamingStats));
	bank.PopGroup();

	bank.PopGroup();	// LOGGING

#endif // !__DEV
	bank.PushGroup("Scene Streamer");
	g_SceneStreamerMgr.AddWidgets(bank);
	bank.AddToggle("Show streaming interests", &bDisplayStreamingInterests);
	bank.AddToggle("Disable churn protection", CStreaming::GetCleanup().GetDisableChurnProtectionPtr());
	bank.AddToggle("Display churn point", &bDisplayChurnPoint);
	bank.AddToggle("Vector map Requested Entities", &g_show_streaming_on_vmap);
	bank.AddToggle("Show bucket info for selected", &bShowBucketInfoForSelected);

	bank.PopGroup();

//#if __DEV
	bank.PushGroup("Framework Engine");
	bank.PushGroup("Extreme Streaming");
	bank.AddToggle("Enable Extreme Streaming", &strStreamingLoader::ms_ExtremeStreaming_Enabled);
	bank.AddSlider("Max Pending Requests Per Device", &strStreamingLoader::ms_MaxPendingRequestThreshold, 1, 30, 1);
	bank.AddSlider("Max Pending (Kb)", &strStreamingLoader::ms_ExtremeStreaming_MaxPendingKb, 1, 100000, 1);
	bank.AddSlider("Max Convert (Ms)", &strStreamingLoader::ms_ExtremeStreaming_MaxConvertMs, 1, 100000, 1);
	bank.AddToggle("Add SV Markers", strStreamingEngine::GetLoader().GetLoaderSVMarkersPtr());
	bank.PopGroup();
//#endif // __DEV

	bank.PushGroup("Request Scheduling");
	bank.AddToggle("Sort by LSN on HDD", strStreamingInfoManager::GetSortOnHDDPtr());
	bank.AddToggle("Fair HDD Scheduler", strStreamingInfoManager::GetFairHDDSchedulerPtr());
	bank.AddSlider("LSN tolerance ODD", &strStreamingLoader::ms_LsnTolerance[pgStreamer::OPTICAL], 0, 0x4000000, 1);
	bank.AddSlider("LSN tolerance HDD", &strStreamingLoader::ms_LsnTolerance[pgStreamer::HARDDRIVE], 0, 0x4000000, 1);
	bank.AddSlider("Max chained requests", &strStreamingLoader::ms_MaxChainedRequests, 1, 512, 1);
	bank.PopGroup();

#if __DEV
	bank.PushGroup("Resource Placement");
#if TRACK_PLACE_STATS
	bank.AddToggle("Show place stats", &bDisplayModulePlaceStats);
	bank.AddButton("Reset place stats", datCallback(ResetPlaceStats));
#endif // TRACK_PLACE_STATS
	strStreamingEngine::GetInfo().GetModuleMgr().AddWidgets(bank);
	bank.PopGroup();
#endif // __DEV
#if __BANK
	bank.AddToggle("Show Loader Status", &bShowLoaderStatus);
	bank.AddToggle("Show Async Stats", &bShowAsyncStats);
#endif // __BANK
#if __DEV
	bank.AddToggle("Pump And Dump", &strStreamingLoader::ms_IsPumpAndDump, NullCB, "Immediately discard a resource after loading it");
	bank.AddToggle("Summon Sir Cancel-A-Lot", &strStreamingLoader::ms_IsSirCancelALot, NullCB, "Cancel a lot of requests in mid-flight");
	bank.AddSlider("Sir Cancel-A-Lot Frequency", &strStreamingLoader::ms_SirCancelALotFrequency, 1, 100, 1);
#endif	//	__DEV
	strPackfileManager::AddWidgets(bank);

	bank.AddButton("Clean scene", datCallback(CleanScene));
	bank.AddToggle("Clean scene every frame", &s_bCleanSceneEveryFrame);
	bank.AddSlider("Memory threshold (%) for garbage collection (Virt)", &s_MemoryGarbageCollectionThresholdVirt, 0, 100, 1);
	bank.AddSlider("Memory threshold (%) for garbage collection (Phys)", &s_MemoryGarbageCollectionThresholdPhys, 0, 100, 1);
	bank.AddSlider("Auto-cleanup cutoff score", &s_AutoCleanupCutoff, 0.0f, 10.0f, 0.001f);
	bank.AddSlider("Garbage collection cutoff score", &s_GarbageCollectionCutoff, 0.0f, 10.0f, 0.001f);
	bank.AddToggle("Garbage collect entities only", &s_GarbageCollectEntitiesOnly);

	bank.PushGroup("Manual Loading/Unloading");
	bank.AddButton("Load all models once", datCallback(LoadAllObjectsOnce));
	bank.AddText("Asset to debug (include extension)", s_TestAssetName, sizeof(s_TestAssetName));
	bank.AddButton("Force-load test asset", datCallback(ForceLoadTestAsset));
	bank.AddButton("Unload test asset", datCallback(UnloadTestAsset));
	bank.PopGroup();

#if STREAM_STATS
	bank.AddToggle("Show Module Streaming Stats", &s_ShowModuleStreamingStats);
	bank.AddButton("Reset Module Streaming Stats", datCallback(ResetModuleStreamingStats));
#endif // STREAM_STATS

	bank.PopGroup();	// FRAMEWORK ENGINE

	bank.PushGroup("Low-Level Streamer");
	bank.AddSlider("Streamer Thread Priority", &s_StreamerThreadPriority, PRIO_LOWEST, PRIO_HIGHEST, 1, UpdateThreadPriority);

#if __DEV
	bank.AddButton("Dump requests", datCallback(DumpRequests));
#endif // __DEV

	bank.PopGroup();

	// Flush control
	CStreaming::GetCleanup().AddWidgets(bank);

#if __DEV
	bank.AddButton("Reset Loaded object counts", datCallback(ResetLoadedCounts));
#endif // __DEV

//	bank.AddToggle("Models Used by Scripts", &bDisplayModelsUsedbyScripts);
//	bank.AddToggle("Per-script model usage", &CScriptDebug::GetDisplayModelUsagePerScript());
//	bank.AddToggle("Count model usage", &CScriptDebug::GetCountModelUsage());
//	bank.AddToggle("Count anim usage", &CScriptDebug::GetCountAnimUsage());
//	bank.AddToggle("Count texture usage", &CScriptDebug::GetCountTextureUsage());

#if __DEV
	bank.AddButton("Check for held obj : interior", &CheckForHeldObjInteriorCB);
	bank.AddButton("Check for held obj : cutscene", &CheckForHeldObjCutsceneCB);
	bank.PopGroup();
#endif //__DEV

	//	CStreaming::GetPopulation().InitWidgets(bank);

#if PRIORITIZED_CLIP_SET_STREAMER_WIDGETS
	CPrioritizedClipSetStreamer::GetInstance().AddWidgets(bank);
#endif

	BANK_ONLY(CZonedAssetManager::InitWidgets(bank));

	bank.AddButton("EBO SPECIAL: Switch to main", &SwitchToMainCCS);
}



///////////////////////////////////////////////////////////////////////////////
// B*519409 - on button output for all resource memory use

char	ResourceMemoryUseDump::m_LogFileName[RAGE_MAX_PATH];
fiStream *ResourceMemoryUseDump::m_pLogFileHandle = NULL;
atArray<strIndex> ResourceMemoryUseDump::m_SceneCostStreamingIndices;
bool	ResourceMemoryUseDump::m_bIncludeDetailedBreakdownByCallstack = true;
bool	ResourceMemoryUseDump::m_bIncludeDetailedBreakdownByContext = true;

void ResourceMemoryUseDump::AddWidgets(bkGroup &group)
{
	bkGroup *pDumpGroup = group.AddGroup("Resource Memory Use");

	pDumpGroup->AddText("Log file", m_LogFileName, RAGE_MAX_PATH);
	pDumpGroup->AddButton("Pick file", SelectLogFileCB);
	pDumpGroup->AddButton("Dump Resource Memory Use to Log", SaveToLogCB);
	pDumpGroup->AddToggle("Include Detailed ExtAlloc Details by Callstack", &m_bIncludeDetailedBreakdownByCallstack);
	pDumpGroup->AddToggle("Include Detailed ExtAlloc Details by Context", &m_bIncludeDetailedBreakdownByContext);
}

void ResourceMemoryUseDump::SelectLogFileCB()
{
	if (!BANKMGR.OpenFile(m_LogFileName, RAGE_MAX_PATH, "*.txt", true, "ResourceMemoryUse log (*.txt)"))
	{
		m_LogFileName[0] = '\0';
	}
}

fiStream *ResourceMemoryUseDump::OpenLogFile()
{
	fiStream *LogStream = m_LogFileName[0] ? ASSET.Create(m_LogFileName, "") : NULL;
	return LogStream;
}

void ResourceMemoryUseDump::CloseLogFile(fiStream *pLogFileHandle)
{
	fflush(pLogFileHandle);
	pLogFileHandle->Close();
}

void ResourceMemoryUseDump::SaveToLogCB()
{
	m_pLogFileHandle = OpenLogFile();
	if( m_pLogFileHandle )
	{
		int sceneCostvRamSize;
		int sceneCostMainRamSize;
		BuildSceneCostStreamingIndices();
		CalcSceneCostTotals(sceneCostMainRamSize, sceneCostvRamSize);

		int requiredvRamSize;
		int requiredMainRamSize;
		BuildLoadedRequiredFilesCosts(requiredMainRamSize,requiredvRamSize);

		int extAllocsvRamSize;
		int extAllocsMainRamSize;
		CalcExtAllocTotals(extAllocsMainRamSize, extAllocsvRamSize);

		fprintf(m_pLogFileHandle, "** START LOG: Totals: (name, main, vram) **\n\n");
		fflush(m_pLogFileHandle);

		// Print the pre-amble form the calculated sizes
		// Scene costs
		fprintf(m_pLogFileHandle, "SceneCost, %dkb, %dkb\n", sceneCostMainRamSize >> 10, sceneCostvRamSize >> 10);
		fflush(m_pLogFileHandle);

		// Required Files
		fprintf(m_pLogFileHandle, "LoadedRequiredFiles, %dkb, %dkb\n", requiredMainRamSize >> 10, requiredvRamSize >> 10);
		fflush(m_pLogFileHandle);

		// External Allocations
		fprintf(m_pLogFileHandle, "ExternalAllocations, %dkb, %dkb\n\n", extAllocsMainRamSize >> 10, extAllocsvRamSize >> 10 );
		fflush(m_pLogFileHandle);

		fprintf(m_pLogFileHandle, "** END LOG: Totals **\n\n");
		fflush(m_pLogFileHandle);

		// Now dump the detailed output for each
		DumpSceneCostDetails(m_pLogFileHandle);
		DumpCurrentlyLoadedRequiredFiles(m_pLogFileHandle);
		DumpExtAllocDetails(m_pLogFileHandle);

		CloseLogFile(m_pLogFileHandle);
	}
	m_pLogFileHandle = NULL;
}


//// SCENE COSTS
// callback for world search code to append a registered entity to list
bool ResourceMemoryUseDump::AppendToListCB(CEntity* pEntity, void* pData)
{
	if (pEntity && pData)
	{
		atArray<CEntity*>* pEntities = (atArray<CEntity*>*) pData;
		pEntities->PushAndGrow(pEntity);
	}
	return true;
}

s32 ResourceMemoryUseDump::SortByNameCB(const strIndex* pIndex1, const strIndex* pIndex2)
{
	char achName1[RAGE_MAX_PATH];
	char achName2[RAGE_MAX_PATH];
	const char *name1 = strStreamingEngine::GetInfo().GetObjectName(*pIndex1, achName1, RAGE_MAX_PATH);
	const char *name2 = strStreamingEngine::GetInfo().GetObjectName(*pIndex2, achName2, RAGE_MAX_PATH);
	return stricmp(name1, name2);
}

void ResourceMemoryUseDump::BuildSceneCostStreamingIndices()
{
	atArray<CEntity*> entityArray;

	atMap<s32, u32>	txdMap;
	atMap<s32, u32>	dwdMap;
	atMap<s32, u32>	drawableMap;
	atMap<s32, u32>	fragMap;

	// perform world search
	CPed* pPlayerPed = CGameWorld::GetMainPlayerInfo()->GetPlayerPed();
	const Vector3 vPlayerPos = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());

	s32 entityTypeFlags = ENTITY_TYPE_MASK_BUILDING | ENTITY_TYPE_MASK_ANIMATED_BUILDING | ENTITY_TYPE_MASK_OBJECT | ENTITY_TYPE_MASK_DUMMY_OBJECT;

	spdSphere sphere(RCC_VEC3V(vPlayerPos), ScalarV(4500.0f));
	fwIsSphereIntersectingVisible searchSphere(sphere);
	CGameWorld::ForAllEntitiesIntersecting(	&searchSphere,
		AppendToListCB,
		(void*) &entityArray,
		entityTypeFlags,
		SEARCH_LOCATION_INTERIORS | SEARCH_LOCATION_EXTERIORS,
		SEARCH_LODTYPE_ALL,
		SEARCH_OPTION_FORCE_PPU_CODEPATH | SEARCH_OPTION_LARGE,
		WORLDREP_SEARCHMODULE_DEFAULT );

	s32 txdModuleId = g_TxdStore.GetStreamingModuleId();
	s32 drawableModuleId = g_DrawableStore.GetStreamingModuleId();
	s32 fragModuleId = g_FragmentStore.GetStreamingModuleId();
	s32 dwdModuleId = g_DwdStore.GetStreamingModuleId();

	strStreamingModule* pTxdStreamingModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(txdModuleId);
	strStreamingModule* pDrawableStreamingModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(drawableModuleId);
	strStreamingModule* pFragStreamingModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(fragModuleId);
	strStreamingModule* pDwdStreamingModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(dwdModuleId);

	m_SceneCostStreamingIndices.Reset();
	for(int i=0;i<entityArray.size();i++)
	{
		CEntity *pEntity = entityArray[i];
		CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();

		// Get txd streaming indices
		strLocalIndex txdDictIndex = strLocalIndex(pModelInfo->GetAssetParentTxdIndex());
		if (txdDictIndex.Get()!=-1)
		{
			u32 * const pResult = txdMap.Access(txdDictIndex.Get());
			if (!pResult)
			{
				// stat hasn't been counted yet
				strIndex txdStreamingIndex = pTxdStreamingModule->GetStreamingIndex(txdDictIndex);
				txdMap.Insert(txdDictIndex.Get(), 1);
				m_SceneCostStreamingIndices.PushAndGrow(txdStreamingIndex);

				// check parent txd
				strLocalIndex parentTxdIndex = g_TxdStore.GetParentTxdSlot(strLocalIndex(txdDictIndex));
				while (parentTxdIndex.Get()!=-1)
				{
					u32 * const pParentResult = txdMap.Access(parentTxdIndex.Get());
					if (!pParentResult)
					{
						// stat hasn't been counted yet
						strIndex parentTxdStreamingIndex = pTxdStreamingModule->GetStreamingIndex(parentTxdIndex);
						txdMap.Insert(parentTxdIndex.Get(), 1);
						m_SceneCostStreamingIndices.PushAndGrow(parentTxdStreamingIndex);
					}
					parentTxdIndex = g_TxdStore.GetParentTxdSlot(parentTxdIndex);
				}
			}
		}

		// Get dwd streaming indices
		strLocalIndex dwdDictIndex = (pModelInfo->GetDrawableType()==fwArchetype::DT_DRAWABLEDICTIONARY) ? strLocalIndex(pModelInfo->GetDrawDictIndex()) : strLocalIndex(INVALID_DRAWDICT_IDX);
		if (dwdDictIndex.Get()!=INVALID_DRAWDICT_IDX)
		{
			u32 * const pResult = dwdMap.Access(dwdDictIndex.Get());
			if (!pResult)
			{
				// stat hasn't been counted yet
				strIndex dwdStreamingIndex = pDwdStreamingModule->GetStreamingIndex(dwdDictIndex);
				dwdMap.Insert(dwdDictIndex.Get(), 1);
				m_SceneCostStreamingIndices.PushAndGrow(dwdStreamingIndex);
			}
		}

		// Get drawable streaming indices
		strLocalIndex drawableIndex = (pModelInfo->GetDrawableType()==fwArchetype::DT_DRAWABLE) ? strLocalIndex(pModelInfo->GetDrawableIndex()) : strLocalIndex(INVALID_DRAWABLE_IDX);
		if (drawableIndex.Get()!=INVALID_DRAWABLE_IDX)
		{
			u32 * const pResult = drawableMap.Access(drawableIndex.Get());
			if (!pResult)
			{
				// stat hasn't been counted yet
				strIndex drawableStreamingIndex = pDrawableStreamingModule->GetStreamingIndex(drawableIndex);
				drawableMap.Insert(drawableIndex.Get(), 1);
				m_SceneCostStreamingIndices.PushAndGrow(drawableStreamingIndex);
			}
		}

		// Get frag streaming indices
		strLocalIndex fragIndex = (pModelInfo->GetDrawableType()==fwArchetype::DT_FRAGMENT) ? strLocalIndex(pModelInfo->GetFragmentIndex()) : strLocalIndex(INVALID_FRAG_IDX);
		if (fragIndex.Get()!=INVALID_FRAG_IDX)
		{
			u32 * const pResult = fragMap.Access(fragIndex.Get());
			if (!pResult)
			{
				// stat hasn't been counted yet
				strIndex fragStreamingIndex = pFragStreamingModule->GetStreamingIndex(fragIndex);
				fragMap.Insert(fragIndex.Get(), 1);
				m_SceneCostStreamingIndices.PushAndGrow(fragStreamingIndex);
			}
		}
	}
}

void ResourceMemoryUseDump::CalcSceneCostTotals(int &mainRam, int &vRam)
{
	mainRam = 0;
	vRam = 0;

	for (s32 i=0; i<m_SceneCostStreamingIndices.size(); i++)
	{
		strIndex index = m_SceneCostStreamingIndices[i];
		//const char* name = strStreamingEngine::GetInfo().GetObjectName(m_SceneCostStreamingIndices[i]);
		strStreamingInfo& info = *strStreamingEngine::GetInfo().GetStreamingInfo(index);

		mainRam += info.ComputeOccupiedVirtualSize(index, true);
		vRam += info.ComputePhysicalSize(index, true);
	}
}


void ResourceMemoryUseDump::DumpSceneCostDetails(fiStream *pLogStream)
{
	m_SceneCostStreamingIndices.QSort(0, -1, SortByNameCB);
	fprintf(pLogStream, "** START LOG: Scene Cost (name, main, vram) **\n\n" );
	fflush(pLogStream);
	for (s32 i=0; i<m_SceneCostStreamingIndices.size(); i++)
	{
		strIndex index = m_SceneCostStreamingIndices[i];
		const char* name = strStreamingEngine::GetInfo().GetObjectName(index);
		strStreamingInfo& info = *strStreamingEngine::GetInfo().GetStreamingInfo(index);
		fprintf(pLogStream, "%s, %dkb, %dkb\n", name, ( info.ComputeOccupiedVirtualSize(index, true) >> 10 ), ( info.ComputePhysicalSize(index, true) >> 10) );
		fflush(pLogStream);
	}
	fprintf(pLogStream, "\n** END LOG: Scene Cost **\n\n");
	fflush(pLogStream);
	m_SceneCostStreamingIndices.Reset();
}



//// REQUIRED FILES
void ResourceMemoryUseDump::BuildLoadedRequiredFilesCosts(int &mainRam, int &vRam)
{
	mainRam = 0;
	vRam = 0;

	u32 nTrackedModules = strStreamingEngine::GetInfo().GetModuleMgr().GetNumberOfModules();
	for(int i=0;i<nTrackedModules;i++)
	{
		strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(i);

		for(s32 objIdx = 0; objIdx < pModule->GetCount(); ++objIdx)
		{
			strStreamingInfoManager & strInfoManager = strStreamingEngine::GetInfo();
			strIndex streamingIndex = pModule->GetStreamingIndex(strLocalIndex(objIdx));
			if ( strInfoManager.IsObjectInImage(streamingIndex) )
			{
				bool bLoaded  = strInfoManager.HasObjectLoaded(streamingIndex);
				if( bLoaded )
				{
					strStreamingInfo* info = strInfoManager.GetStreamingInfo(streamingIndex);
					bool bReferenced = ( pModule->GetNumRefs( strLocalIndex(objIdx) ) > 0 || info->GetDependentCount() > 0 );
					bool bRequired = pModule->IsObjectRequired( strLocalIndex(objIdx) );

					if( bRequired || bReferenced )
					{
						mainRam += info->ComputeOccupiedVirtualSize(streamingIndex, true);
						vRam += info->ComputePhysicalSize(streamingIndex, true);
					}
				}
			}
		}
	}


}

void ResourceMemoryUseDump::DumpCurrentlyLoadedRequiredFiles(fiStream *pLogStream)
{

	fprintf(pLogStream, "** START LOG: Loaded Required Files Cost (name, main, vram) **\n\n" );
	fflush(pLogStream);

	u32 nTrackedModules = strStreamingEngine::GetInfo().GetModuleMgr().GetNumberOfModules();
	for(int i=0;i<nTrackedModules;i++)
	{
		strStreamingModule* pModule = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(i);

		for(s32 objIdx = 0; objIdx < pModule->GetCount(); ++objIdx)
		{
			strStreamingInfoManager & strInfoManager = strStreamingEngine::GetInfo();
			strIndex streamingIndex = pModule->GetStreamingIndex(strLocalIndex(objIdx));
			if ( strInfoManager.IsObjectInImage(streamingIndex) )
			{
				bool bLoaded  = strInfoManager.HasObjectLoaded(streamingIndex);
				if( bLoaded )
				{
					strStreamingInfo* info = strInfoManager.GetStreamingInfo(streamingIndex);
					bool bReferenced = ( pModule->GetNumRefs( strLocalIndex(objIdx) ) > 0 || info->GetDependentCount() > 0 );
					bool bRequired = pModule->IsObjectRequired( strLocalIndex(objIdx) );

					if( bRequired || bReferenced )
					{
						const char* name = strInfoManager.GetObjectName(streamingIndex);
						fprintf(pLogStream, "%s, %dkb, %dkb\n", name, ( info->ComputeOccupiedVirtualSize(streamingIndex, true) >> 10 ), ( info->ComputePhysicalSize(streamingIndex, true) >> 10) );
					}
				}
			}
		}
	}
	fprintf(pLogStream, "\n** END LOG: Loaded Required Files Cost **\n\n");
	fflush(pLogStream);
}


//// EXTERNAL ALLOCATIONS
int ResourceMemoryUseDump::CompareExtAllocByAddress(const strExternalAlloc* pA, const strExternalAlloc* pB)
{
	ptrdiff_t diff = (char*)pA->m_pMemory - (char*)pB->m_pMemory;
	return diff? (diff>0? 1 : -1) : 0;
}

int ResourceMemoryUseDump::CompareExtAllocByStackTracehandle(const strExternalAlloc* pA, const strExternalAlloc* pB)
{
	return (int)pA->m_BackTraceHandle - (int)pB->m_BackTraceHandle;
}

void ResourceMemoryUseDump::CalcExtAllocTotals(int &mainRam, int &vRam)
{
	mainRam = 0;
	vRam = 0;

	atArray<strExternalAlloc> &Allocs = strStreamingEngine::GetAllocator().m_ExtAllocTracker.m_Allocations;

	for(int i=0;i<Allocs.size();i++)
	{
		if( Allocs[i].IsMAIN() )
		{
			// Yes
			mainRam += Allocs[i].m_Size;
		}
		else
		{
			// No
			vRam += Allocs[i].m_Size;
		}
	}
}

void ResourceMemoryUseDump::DumpExtAllocDetails(fiStream *pLogStream)
{
	atArray<strExternalAlloc> AllocsInBucket;
	atArray<strExternalAlloc> &Allocs = strStreamingEngine::GetAllocator().m_ExtAllocTracker.m_Allocations;

	if( Allocs.size() )
	{
		fprintf(pLogStream, "** START LOG: External Allocations (bucket, main, vram) **\n\n" );
		fflush(pLogStream);

		for(int b=0;b<(MEMBUCKET_DEBUG+1);b++)
		{
			AllocsInBucket.Reset();
			for(int i=0;i<Allocs.size();i++)
			{
				if( Allocs[i].m_BucketID == b)
				{
					AllocsInBucket.PushAndGrow(Allocs[i]);
				}
			}

			if(AllocsInBucket.size())
			{

				// Totals for this bucket
				int	mainRam = 0;
				int vRam = 0;
				for(int i=0;i<AllocsInBucket.size();i++)
				{
					// Is this Virtual?
					if( AllocsInBucket[i].IsMAIN() )
					{
						// Yes
						mainRam += AllocsInBucket[i].m_Size;
					}
					else
					{
						// No
						vRam += AllocsInBucket[i].m_Size;
					}
					fflush(pLogStream);
				}
				fprintf(pLogStream, "%s, %dkb, %dkb\n", strStreamingEngine::GetAllocator().GetBucketName(b), mainRam>>10, vRam>>10 );
				fflush(pLogStream);
			}
		}

		// Now do the more detailed output for each bucket
		for(int b=0;b<(MEMBUCKET_DEBUG+1);b++)
		{
			AllocsInBucket.Reset();
			for(int i=0;i<Allocs.size();i++)
			{
				if( Allocs[i].m_BucketID == b)
				{
					AllocsInBucket.PushAndGrow(Allocs[i]);
				}
			}

			// Output detailed information using the callstack
			if(AllocsInBucket.size() && m_bIncludeDetailedBreakdownByCallstack)
			{
				fprintf(pLogStream, "\n**BUCKET %s: Detailed Breakdown (CALLSTACK)\n\n", strStreamingEngine::GetAllocator().GetBucketName(b));
				fflush(pLogStream);

				// Group allocs together by callstack backtrace handle
				AllocsInBucket.QSort(0,-1,CompareExtAllocByStackTracehandle);
				int index = 0;
				bool done = false;
				while(!done)
				{
					int mainRam = 0;
					int vRam = 0;
					u16 currentTraceHandle = AllocsInBucket[index].m_BackTraceHandle;
					while( currentTraceHandle == AllocsInBucket[index].m_BackTraceHandle )
					{
						if( AllocsInBucket[index].IsMAIN() )
						{
							mainRam += AllocsInBucket[index].m_Size;
						}
						else
						{
							vRam += AllocsInBucket[index].m_Size;
						}
						index++;
						if( index >= AllocsInBucket.size() )
						{
							done = true;
							break;
						}
					}
					sysStack::PrintRegisteredBacktrace(currentTraceHandle, WriteBacktraceLine);
					fprintf(pLogStream, ", %dkb, %dkb\n", mainRam>>10, vRam>>10 );
				}

/*
				// Print all the allocs
				for(int i=0;i<AllocsInBucket.size();i++)
				{
					sysStack::PrintRegisteredBacktrace(AllocsInBucket[i].m_BackTraceHandle, WriteBacktraceLine);
					if( AllocsInBucket[i].IsMAIN() )
					{
						fprintf(pLogStream, ", %dkb, %dkb\n", AllocsInBucket[i].m_Size>>10, 0 );
					}
					else
					{
						fprintf(pLogStream, ", %dkb, %dkb\n", 0, AllocsInBucket[i].m_Size>>10 );
					}
					fflush(pLogStream);
				}
*/


				fprintf(pLogStream, "\n**BUCKET %s: End Detailed Breakdown (CALLSTACK)\n", strStreamingEngine::GetAllocator().GetBucketName(b));
				fflush(pLogStream);
			}

			// Output detailed information using the captured diagContextMessage
			if(AllocsInBucket.size() && m_bIncludeDetailedBreakdownByContext)
			{
				fprintf(pLogStream, "\n**BUCKET %s: Detailed Breakdown (CONTEXT MESSAGE)\n\n", strStreamingEngine::GetAllocator().GetBucketName(b));
				fflush(pLogStream);

				for(int i=0;i<AllocsInBucket.size();i++)
				{
					//fprintf(pLogStream, "ContextMessage:- ");
					//fflush(pLogStream);

					u32 hash = AllocsInBucket[i].m_diagContextMessageHash;
					atDiagHashString theString(hash);
					const char *pText = theString.GetCStr();
					if(pText)
					{
						fprintf(pLogStream, "Context:%s", pText);
					}
					else
					{
						fprintf(pLogStream, "Context:UNKNOWN(addr:0x%p)", AllocsInBucket[i].m_pMemory );
					}

					if( AllocsInBucket[i].IsMAIN() )
					{
						fprintf(pLogStream, ", %dkb, %dkb\n", AllocsInBucket[i].m_Size>>10, 0 );
					}
					else
					{
						fprintf(pLogStream, ", %dkb, %dkb\n", 0, AllocsInBucket[i].m_Size>>10 );
					}
					fflush(pLogStream);
				}

				fprintf(pLogStream, "\n**BUCKET %s: End Detailed Breakdown (CONTEXT MESSAGE)\n", strStreamingEngine::GetAllocator().GetBucketName(b));
				fflush(pLogStream);

			}
		}

		fprintf(pLogStream, "\n** END LOG: External Allocations **\n\n");
		fflush(pLogStream);
	}
}

// Chopped down to just the symbol
void ResourceMemoryUseDump::WriteBacktraceLine(size_t UNUSED_PARAM(addr),const char* sym,size_t UNUSED_PARAM(displacement))
{
	fiStream *pFileHandle = GetLogFileHandle();
	if( pFileHandle )
	{
		char line[1024];
//		sprintf(line,"0x%"SIZETFMT"x - %s - %"SIZETFMT"u", addr, sym, displacement );
		sprintf(line,"%s|",sym);
		fprintf(pFileHandle, line);
		fflush(pFileHandle);
	}
}

///////////////////////////////////////////////////////////////////////////////

#endif	//__BANK

void CStreamingDebug::DumpModuleMemoryComparedToAverage()
{
#if __BANK
	if (GetMemoryProfileLocationNames() == NULL)
	{
		LoadMemoryProfileLocations();
	}

	if (CStreamingDebug::GetMemoryProfileLocationList().m_Locations.GetCount() > 0)
	{
		const MemoryFootprint &footprint = CStreamingDebug::GetMemoryProfileLocationList().m_Locations[0].m_MemoryFootprint;
		strStreamingModuleMgr &mgr = strStreamingEngine::GetInfo().GetModuleMgr();

		int moduleCount = mgr.GetNumberOfModules();

		streamDisplayf("MODULE\tUsage (Virt KB)\tUsage (Phys KB)\tAverage (Virt KB)\tAverage (Phys KB)\tDiff (Virt KB)\tDiff (Phys KB)");

		for (int x=0; x<moduleCount; x++)
		{
			strStreamingModule *module = mgr.GetModule(x);
			const MemoryProfileModuleStat *stat = footprint.m_ModuleMemoryStatsMap.SafeGet(atStringHash(module->GetModuleName()));

			if (stat)
			{
				int objCount = module->GetCount();
				size_t virtCount = 0;
				size_t physCount = 0;

				size_t avgVirt = stat->m_VirtualMemory;
				size_t avgPhys = stat->m_PhysicalMemory;

				for(s32 objIdx = 0; objIdx < objCount; ++objIdx)
				{
					strStreamingInfo * info = module->GetStreamingInfo(strLocalIndex(objIdx));
					strIndex streamIndex = module->GetStreamingIndex(strLocalIndex(objIdx));

					switch (info->GetStatus())
					{
					case STRINFO_LOADING:
						virtCount += (size_t) info->ComputeOccupiedVirtualSize(streamIndex, true);
						physCount += (size_t) info->ComputePhysicalSize(streamIndex, true);
						break;

					case STRINFO_LOADED:
						virtCount += (size_t) module->GetVirtualMemoryOfLoadedObj(strLocalIndex(objIdx), false);
						physCount += (size_t) module->GetPhysicalMemoryOfLoadedObj(strLocalIndex(objIdx), false);
						break;

					default:
						break;
					}
				}

				virtCount /= 1024;
				physCount /= 1024;

				streamDisplayf("%s\t%" SIZETFMT "d\t%" SIZETFMT "d\t%" SIZETFMT "d\t%" SIZETFMT "d\t%" SIZETFMT "d\t%" SIZETFMT "d",
					module->GetModuleName(),
					virtCount, physCount,
					avgVirt, avgPhys,
					virtCount - avgVirt, physCount - avgPhys);
			}
		}
	}
#endif // __BANK
}


