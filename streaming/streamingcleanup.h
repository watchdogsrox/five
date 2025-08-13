//
// streaming/streamingcleanup.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef STREAMING_STREAMINGCLEANUP_H
#define STREAMING_STREAMINGCLEANUP_H

#include "data/base.h"

// game headers
#include "fwtl\linklist.h"
#include "scene/streamer/SceneScoring.h"
#include "streaming/streamingdefs.h"


namespace rage
{
	class bkGroup;
	class fwEntity;
	class rmcDrawable;
}

struct CStreamingBucketSet;
struct BucketEntry;
struct DeletionMap;
struct StreamingEntry;

// Doubt we'll ship with that - nosim is faster and has been in use for the past few years
#define SUPPORT_STREAMING_SIM_MODE		(0)

//
// class containing methods for cleaning up scene
//
class CStreamingCleanup : public datBase
{
public:
	enum RecurseResult
	{
		MUST_ABORT = -1,
		NOTHING_DELETED = 0,
		DELETED_DUMMIES = 1,
		FREED_MEMORY = 2,
	};

	// Memory clean-up result
	enum CleanupResult
	{
		CLEANUP_ABORTED = -1,				// Allocation was aborted since 
		CLEANUP_FAILED = 0,					// Not enough memory could be freed up
		CLEANUP_SUCCESS = 1,				// Cleanup was able to free up memory
	};



	void Init();
	void Shutdown();
	void RequestFlush(bool loadscene);
	void RequestFlushNonMission(bool scriptsOnly);
	bool HasFlushBeenProcessed() { return m_flushProcessed; }
	void Update();

	bool HasSpaceFor(strStreamingAllocator& allocator, size_t virtualSize, size_t physicalSize);

#if RSG_PC
	size_t GetPhysicalSize(datResourceMap &map);
	size_t GetVirtualSize(datResourceMap &map);
	bool HandleOutOfMemory(size_t virtualSize, size_t physicalSize);
#endif // RSG_PC

	bool MakeSpaceForMap(datResourceMap& map, strIndex index, bool keepMemory);
	bool MakeSpaceForResourceFile(const char* filename, const char* ext, s32 version);

	// PURPOSE: Check to see whether the last call to MakeSpaceForMap() was aborted because
	// it was determined that the object isn't worth freeing memory for. This flag will only
	// be updated after calling MakeSpaceForMap() with a valid strIndex.
	bool WasLastAllocationAborted() const;

	// PURPOSE: Check to see whether the last call to MakeSpaceForMap() on the main
	// thread was unsuccessful due to fragmentation.
	bool WasLastAllocationFragmented() const;

	// PURPOSE: Delete all entities that are not in use and unlikely to be needed again
	// in the next few frames. This can be called whenever the entity count is getting too high
	// for comfort.
	//
	// PARAMS:
	//  maxObjsToDelete - Maximum number of entities that may be deleted.
	//  mustDelete - Asserts if we didn't delete at least one entity.
	//  scoreCutoff - Only entities with a score equal or less than this value will be deleted.
	//
	// RETURNS: The number of entities that were deleted.
	int DeleteUnusedEntities(int maxObjsToDelete, bool mustDelete, float scoreCutoff);

	// PURPOSE: Dump out all active entities and assert. This should be called when we have
	// tons of active entities and want to find out why.
	void AssertAndDumpEntities();

	// PURPOSE: If we have a critical number of entities, delete some of the ones we think
	// won't be missed - we'll assert if we do though.
	void RemoveExcessEntities();

	void DeleteAllDrawableReferences();
	bool PurgeDrawable(rmcDrawable* pDrawable);
	
#if STREAMING_DETAILED_STATS
	void MarkDrawHandlerDeleted(CEntity *entity, float score);
	void MarkDrawHandlerCreated(CEntity *entity, float score);

	void DisplayChurnStats();
#endif // STREAMING_DETAILED_STATS

#if __BANK
	void DisplayChurnPoint();
	bool *GetDisableChurnProtectionPtr()			{ return &m_DisableChurnProtection; }
	void AddWidgets(bkGroup &group);
	void UpdateNonClipsFlushControls();
	void UpdateClipsFlushControls();
#endif // __BANK

#if SUPPORT_STREAMING_SIM_MODE && __BANK
	inline bool IsSceneStreamingNoSim()	{ return m_enableStreamingSystemNoSim; }
#else // __BANK
	inline bool IsSceneStreamingNoSim()	{ return true; }
#endif // __BANK


private:
	void Flush();
	bool DeleteLeastUsed(u32 ignoreFlags, s32& stage);
	bool DeleteVehiclesAndPeds(bool bCutscene);

	// Tell all logs and debug channels that we didn't load something due to churnProtection.
	void LogChurnProtectionEvent(BucketEntry &bucketEntry);

	CleanupResult FindLowestScoringEntityToDelete(strIndex indexNeedingMemory, u32 ignoreFlags);
	RecurseResult DeleteEntityAndDependencies(DeletionMap &deletionMap, BucketEntry *bucketEntry, CEntity *entity, strIndex indexNeedingMemory, u32 ignoreFlags, CStreamingBucketSet &bucketSet);
	RecurseResult RecurseDeleteDependencies(strIndex index, strIndex indexNeedingMemory, u32 ignoreFlags);

#if !__FINAL
	// Called after a failed allocation due to complete lack of memory (not because an allocation was aborted because it wasn't important)
	void MarkFailedAllocation();
#endif // !__FINAL

	static bool PerformRemoval(strIndex index, u32 ignoreFlags, int &virtualMemoryToFree, int &physicalMemoryToFree, strIndex indexNeedingMemory, bool *mustAbort);

#if SUPPORT_STREAMING_SIM_MODE
	static bool SimulateRemovalNew(DeletionMap &deletionMap, StreamingEntry *entry, int &virtualMemoryToFree, int &physicalMemoryToFree);
	static StreamingEntry* AddCandidateNew(DeletionMap &deletionMap, BucketEntry *bucketEntry, strIndex index, u32 ignoreFlags, StreamingEntry *parent, int &virtualMemoryToFree, int &physicalMemoryToFree, strIndex indexNeedingMemory, bool *mustAbort);
#endif // SUPPORT_STREAMING_SIM_MODE

#if STREAM_STATS
	int		m_numCleanupCalls;
	int		m_numValidationAttempts;
#endif 

#if STREAMING_DETAILED_STATS
	enum
	{
		MAX_DELETED_COUNT = 128,
		MAX_DELETED_CHURN_FRAMES = 3,
	};

	struct DeletedDrawableHandler
	{
		// The bucket score of the entity
		float m_BucketScore;
	};


	atSimplePooledMapType<CEntity *, DeletedDrawableHandler> m_LastDeletedHandlers[MAX_DELETED_CHURN_FRAMES];
	int m_LastDeletedHandlerIndex;
	int m_ChurnCounts[MAX_DELETED_CHURN_FRAMES];
		
#endif // STREAMING_DETAILED_STATS

#if !__FINAL
	u32		m_LastFailedAlloc;
#endif // !__FINAL

	bool	m_flushRequested;
	bool	m_loadSceneAfterFlush;
	bool	m_flushProcessed;

	// True if the last clean-up with a valid strIndex was aborted. See WasLastAllocationAborted().
	bool	m_LastAllocationWasAborted;

	// True if the last failed allocation on the main thread unsuccessful due to fragmention.
	bool	m_LastAllocationWasFragmented;

	// If true, the map store will be flushed during a flush
	bool	m_FlushMapStore;
	bool	m_FlushVehicles;
	bool	m_FlushPeds;
	bool	m_FlushCollisions;
	bool	m_FlushClips;
	bool	m_FlushObjects;
	bool	m_FlushCloth;
	bool	m_FlushClouds;

#if __BANK
	// If true, we won't turn an entity stream request down to prevent churn.
	bool	m_DisableChurnProtection;

	// Most recent churn point.
	float	m_LastChurnPoint;

	// Timestamp on when the last churn occurred.
	u32		m_LastChurnTimestamp;

#if SUPPORT_STREAMING_SIM_MODE
	bool	m_enableStreamingSystemNoSim;
#endif // SUPPORT_STREAMING_SIM_MODE

#endif // __BANK
};

#endif // STREAMING_STREAMINGCLEANUP_H
