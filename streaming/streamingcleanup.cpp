//
// streaming/streamingcleanup.cpp
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//

#include "streaming/streaming_channel.h"

STREAMING_OPTIMISATIONS()

#include "fwanimation/pointcloud.h"
#include "camera/caminterface.h"
#include "camera/helpers/Frame.h"
#include "camera/viewports/viewportmanager.h"
#include "cutscene/CutSceneManagerNew.h"
#include "fwdebug/vectormap.h"
#include "fwscene/stores/mapdatastore.h"
#include "fwscene/stores/staticboundsstore.h"
#include "fwpheffects/ropemanager.h"
#include "game/config.h"
#include "grcore/resourcecache.h"
#include "peds/ped.h"
#include "peds/rendering/PedVariationPack.h"
#include "renderer/renderer.h"
#include "scene/ContinuityMgr.h"
#include "scene/FocusEntity.h"
#include "scene/lod/LodMgr.h"
#include "scene/streamer/SceneStreamerList.h"
#include "scene/streamer/SceneStreamerMgr.h"
#include "scene/texLod.h"
#include "scene/debug/SceneStreamerDebug.h"
#include "scene/world/gameworld.h"
#include "scene/EntityIterator.h"
#include "script/streamedscripts.h"
#include "streaming/packfilemanager.h"
#include "streaming/populationstreaming.h"
#include "streaming/streaming.h"
#include "streaming/streamingcleanup.h"
#include "streaming/streamingengine.h"
#include "streaming/streaminginfo.h"
#include "streaming/streamingmodule.h"
#include "streaming/streamingvisualize.h"
#include "streaming/zonedassetmanager.h"
#include "script/commands_player.h"
#include "script/commands_ped.h"
#include "system/buddyallocator.h"
#include "system/controlMgr.h"
#include "system/param.h"
#include "pathserver/ExportCollision.h"

// cleanup related
#include "animation/debug/AnimPlacementTool.h"
#include "animation/debug/AnimViewer.h"
#include "animation/debug/ClipEditor.h"
#include "debug/TextureViewer/TextureViewer.h"
#include "fwsys/metadatastore.h"
#include "Game/Dispatch/DispatchManager.h"
#include "pickups/PickupManager.h"
#include "peds/playerinfo.h"
#include "peds/PedFactory.h"
#include "scene/world/gameworld.h"
#include "vehicles/vehicle.h"
#include "vehicles/vehiclepopulation.h"
#include "peds/pedpopulation.h"
#include "vehicles/train.h"
#include "fwtl/pool.h"
#include "peds/ped.h"
#include "objects/object.h"
#include "objects/objectpopulation.h"
#include "renderer/gtadrawable.h"
#include "Peds/PedFactory.h"
#include "fragment/cachemanager.h"
#include "core/game.h"
#include "system/controlMgr.h"
#include "vfx/clouds/CloudHat.h"

#include "data/aes_init.h"
AES_INIT_B;

#if __STATS
PF_PAGE(StreamCleanupPage, "Streaming Cleanup");
PF_GROUP(StreamCleanup);
PF_LINK(StreamCleanupPage, StreamCleanup);

PF_TIMER(MakeSpaceForMap, StreamCleanup);
PF_TIMER(FindLowestScoring, StreamCleanup);
PF_TIMER(AbsorbPVS, StreamCleanup);
PF_TIMER(SortBuckets, StreamCleanup);
PF_TIMER(WaitForSPUSort, StreamCleanup);
PF_TIMER(IssueRequests, StreamCleanup);

#if STREAM_STATS
PF_VALUE_INT(AddedEntities, StreamCleanup);
PF_VALUE_INT(RemovedEntities, StreamCleanup);
PF_VALUE_INT(Entities, StreamCleanup);
PF_VALUE_INT(DeletedEntities, StreamCleanup);
PF_VALUE_INT(NewRemoved, StreamCleanup);
PF_VALUE_INT(NewDeleted, StreamCleanup);
PF_VALUE_INT(CleanupCalls, StreamCleanup);
PF_VALUE_INT(ValidationAttempts, StreamCleanup);
PF_VALUE_INT(ActiveEntities, StreamCleanup);
PF_VALUE_INT(NeededEntities, StreamCleanup);
PF_VALUE_INT(PriorityRequestsODD, StreamCleanup);
PF_VALUE_INT(SecondaryRequestsODD, StreamCleanup);
PF_VALUE_INT(PriorityRequestsHDD, StreamCleanup);
PF_VALUE_INT(SecondaryRequestsHDD, StreamCleanup);

static int s_numRemoved;
static int s_numDeleted;

#endif // STREAM_STATS
#endif // __STATS

#if SUPPORT_STREAMING_SIM_MODE
PARAM( streamingcleanupsim, "Use the simulation step in the new streaming system" );
#endif // SUPPORT_STREAMING_SIM_MODE

#if RSG_PC
namespace rage 
{
	XPARAM( disableExtraMem );
}
#endif 

extern void InterruptInitialize();
extern void InterruptEnd();
extern	bool g_show_streaming_on_vmap;

#define OBJ_INSTANCES_LIMIT	(CSceneStreamerList::MAX_OBJ_INSTANCES - 300)

static int s_CurrentThreshold = OBJ_INSTANCES_LIMIT - 3000;
float s_AutoCleanupCutoff = 0.95f;
static int s_MaxObjsToAutoCleanup = 70;
static bool s_ForceAutoCleanup;

// Percentage threshold that dictates when garbage collection should kick in - if either
// resource heap is full by more than this number (in percent), we'll toss out some entities.
int s_MemoryGarbageCollectionThresholdVirt = 98;
int s_MemoryGarbageCollectionThresholdPhys = 90;
float s_GarbageCollectionCutoff = 0.999999f;
bool s_GarbageCollectEntitiesOnly = false;


struct StreamingEntry
{
	int			m_virtualSize;
	int			m_physicalSize;
	short		m_refCount;
	short		m_RealRefCount;
	short		m_RealDepCount;
	bool		m_IsWorthDeleting;
	char		m_ReferenceCount;
	StreamingEntry **m_References;
	StreamingEntry *m_Parent;
	strIndex	m_Index;
	int			m_Status;

	// If true, we determined that this resource can be deleted.
	bool ShouldDelete() const					{ return m_refCount == m_RealRefCount; }
	bool IsWorthDeleting() const				{ return m_IsWorthDeleting; }

};

struct DeletionMap
{
	enum
	{
		// Total number of possible references for all elements
		REF_BUFFER_SIZE = 8192,

		// Total number of streamables being tracked
		MAX_TRACKED_STREAMABLES = 4096,
	};

	DeletionMap();

	void Reset();

#if SUPPORT_STREAMING_SIM_MODE
	void ExecuteDeletion();

	atFixedArray<BucketEntry *, MAX_TRACKED_STREAMABLES> m_EntitiesToDelete;
	atFixedArray<strIndex, MAX_TRACKED_STREAMABLES> m_ObjectsToRemove;
	atSimplePooledMapType<strIndex, StreamingEntry> m_StreamableMap;
#endif // SUPPORT_STREAMING_SIM_MODE

	u32 m_Timestamp;		// Timestamp of the buckets in the scene streamer manager
	int m_ActiveIndex;
	int m_NeededIndex;

#if SUPPORT_STREAMING_SIM_MODE
	StreamingEntry *m_ReferenceBuffer[REF_BUFFER_SIZE];	
	int m_ReferenceBufferCursor;
#endif // SUPPORT_STREAMING_SIM_MODE
};

static DeletionMap s_DeletionMap;



#define DISTANCE_WE_DONT_DELETE_OBJECTS_WITH_LIGHTS	30.0f


// --- DeletionMap -------------------------------------------------------------------------------------

DeletionMap::DeletionMap()
{
#if SUPPORT_STREAMING_SIM_MODE
	m_StreamableMap.Init(DeletionMap::MAX_TRACKED_STREAMABLES);
#endif // SUPPORT_STREAMING_SIM_MODE

	Reset();
	m_Timestamp = ~0U;
}

void DeletionMap::Reset()
{
#if SUPPORT_STREAMING_SIM_MODE
	m_ReferenceBufferCursor = 0;
#endif // SUPPORT_STREAMING_SIM_MODE
	m_ActiveIndex = 0;
	m_NeededIndex = 0;
#if SUPPORT_STREAMING_SIM_MODE
	m_StreamableMap.Reset();
	m_ObjectsToRemove.Resize(0);
	m_EntitiesToDelete.Resize(0);
#endif // SUPPORT_STREAMING_SIM_MODE
}

#if SUPPORT_STREAMING_SIM_MODE
void DeletionMap::ExecuteDeletion()
{
	int objCount = m_ObjectsToRemove.GetCount();

	// Short-circuit: If we didn't delete anything, there definitely won't be any
	// entities to get rid of.
	if (!objCount)
	{
		return;
	}


	// Go through the entities and see which one actually contributed to the freeing up of
	// memory.
	strStreamingModule *streamingModule = fwArchetypeManager::GetStreamingModule();
	int entityCount = m_EntitiesToDelete.GetCount();

	// We need to put objects we didn't remove back into the list.
	int newIndex = 0;
	strIndex *leftOverObjects = Alloca(strIndex, objCount);

	for (int x=0; x<entityCount; x++)
	{
		CEntity *entity = m_EntitiesToDelete[x]->GetEntity();
		streamAssert(entity);	// Why did we put an empty entity on this list?

		u32 objIndex = entity->GetModelIndex();

		strIndex archIndex = streamingModule->GetStreamingIndex(objIndex);

		StreamingEntry* entry = m_StreamableMap.Access(archIndex);

		// Could be NULL if the archetype had flags that match the ignoreFlags.
		if (entry)
		{
			bool worthDeleting = entry->IsWorthDeleting();

			if (!worthDeleting)
			{

				int refs = entry->m_ReferenceCount;
				for (int y=0; y<refs; y++)
				{
					if (entry->m_References[y]->ShouldDelete())
					{
						worthDeleting = true;
						break;
					}
				}
			}
			// If deleting this entity will save us memory, do it.
			if (worthDeleting)
			{
//				streamDisplayf("ACTUAL DELETE: Entity %p: %s", entity, entity->GetModelName());
#if STREAMING_DETAILED_STATS
				// TODO: Get the score!
				CStreaming::GetCleanup().MarkDrawHandlerDeleted(entity, -4.0f);
#endif // STREAMING_DETAILED_STATS
				entity->DeleteDrawable();
#if __BANK
				g_SceneStreamerMgr.GetStreamingLists().GetActiveEntityList().GetBucketSet().TrackRemoval();
#endif // __BANK
#if STREAM_STATS
				s_numRemoved++;
#endif // STREAM_STATS

				// Don't process this entity again.
				m_EntitiesToDelete.DeleteFast(x);
				x--;
				entityCount--;
			}
			else
			{
				//streamDisplayf("Not worth deleting: Entity %p: %s. Removing references.", entity, entity->GetModelName());
				// Didn't save anything, so we may as well not delete it.
				for (int x=0; x<objCount; x++)
				{
					if (m_ObjectsToRemove[x] == archIndex)
					{
						leftOverObjects[newIndex++] = archIndex;
						// TODO: Null instead of Delete()?
						m_ObjectsToRemove.Delete(x);
						objCount--;
						break;
					}
				}
			}
		}
	}
	
	// Now delete the actual streamables. This must be in the proper order so
	// we handle dependencies correctly.
#if STREAM_STATS
	s_numDeleted += objCount;
#endif // STREAM_STATS

	for (int x=0; x<objCount; x++)
	{
#if __ASSERT
		strIndex index = m_ObjectsToRemove[x];
		strStreamingModule*		module = strStreamingEngine::GetInfo().GetModule( index );
		s32						objIndex = module->GetObjectIndex( index );

//		streamDisplayf("ACTUAL DELETE: Object %d: %s, refs=%d, deps=%d", m_ObjectsToRemove[x].Get(), strStreamingEngine::GetObjectName(m_ObjectsToRemove[x]), module->GetNumRefs(objIndex),
//			strStreamingEngine::GetInfo().GetStreamingInfoRef(index).GetDependentCount());


		if (streamVerifyf(module->GetNumRefs(objIndex) == 0, "Object %d %s (%s) still has a reference count of %d even though we thought it's safe to delete.",
			index.Get(), strStreamingEngine::GetObjectName(index), module->GetModuleName(), module->GetNumRefs(objIndex)))
#endif // __ASSERT
		{
			strStreamingEngine::GetInfo().RemoveObject(m_ObjectsToRemove[x]);
		}
	}

	// Put all those back we didn't delete this time - we may delete them next time.
	if (newIndex)
	{
		sysMemCpy(m_ObjectsToRemove.GetElements(), leftOverObjects, sizeof(strIndex) * newIndex);
	}

	m_ObjectsToRemove.Resize(newIndex);
}
#endif // SUPPORT_STREAMING_SIM_MODE

// --- CStreamingCleanup -------------------------------------------------------------------------------

//
// name:		CStreamingCleanup::Init
// description:	Initialise streaming cleanup class
void CStreamingCleanup::Init()
{
	m_LastAllocationWasAborted = false;
	m_LastAllocationWasFragmented = false;
	m_flushRequested = false;
	m_flushProcessed = false;

	m_FlushClips = true;
	m_FlushCollisions = true;
	m_FlushMapStore = true;
	m_FlushPeds = true;
	m_FlushVehicles = true;
	m_FlushObjects = true;
	m_FlushCloth = true;
	m_FlushClouds = true;

#if !__64BIT && __BANK
	NOTFINAL_ONLY(InterruptInitialize());
#endif

#if __BANK && SUPPORT_STREAMING_SIM_MODE
	m_enableStreamingSystemNoSim = !PARAM_streamingcleanupsim.Get();
#endif //

#if __BANK
	m_DisableChurnProtection = false;
	m_LastChurnTimestamp  = 0;
	m_LastChurnPoint = 0.0f;
#endif // __BANK

#if STREAM_STATS
	s_numRemoved = 0;
	s_numDeleted = 0;
	m_numCleanupCalls = 0;
	m_numValidationAttempts = 0;
#endif // STREAM_STATS

#if STREAMING_DETAILED_STATS
	m_LastDeletedHandlerIndex = 0;

	for (int x=0; x<MAX_DELETED_CHURN_FRAMES; x++)
	{
		m_LastDeletedHandlers[x].Init(MAX_DELETED_COUNT);
	}

	sysMemSet(m_ChurnCounts, 0, sizeof(m_ChurnCounts));
#endif // STREAMING_DETAILED_STATS
}

//
// name:		CStreamingCleanup::Shutdown
// description:	Shutdown streaming cleanup class
void CStreamingCleanup::Shutdown()
{
#if !__64BIT
	NOTFINAL_ONLY(InterruptEnd());
#endif
}



//
// name:		CStreamingCleanup::Update
// description:	Do a continual cleanup
void CStreamingCleanup::Update()
{
//	m_drawableList.DeleteTheLastUsedDrawable();
	
	if (m_flushRequested)
	{
		m_flushRequested = false;
		Flush();
	}
#if STREAM_STATS
	PF_SET(NewRemoved, s_numRemoved);
	PF_SET(NewDeleted, s_numDeleted);
	PF_SET(CleanupCalls, m_numCleanupCalls);
	PF_SET(ValidationAttempts, m_numValidationAttempts);

	s_numRemoved = 0;
	s_numDeleted = 0;
	m_numCleanupCalls = 0;
	m_numValidationAttempts = 0;
#endif 

#if STREAMING_DETAILED_STATS
	m_LastDeletedHandlerIndex++;
	m_LastDeletedHandlerIndex %= MAX_DELETED_CHURN_FRAMES;
	m_LastDeletedHandlers[m_LastDeletedHandlerIndex].Reset();
#endif // STREAMING_DETAILED_STATS

#if !__FINAL && !__64BIT
	if (CControlMgr::GetKeyboard().GetKeyDown(KEY_F, KEYBOARD_MODE_DEBUG_CNTRL_ALT, "Interrupt"))
	{
		InterruptEnd();
	}
#endif // !__FINAL
}

//
// name:		CStreamingCleanup::CDrawableList::DeleteAllDrawableReferences
// description:	Delete all entity references to modelinfo drawables
void CStreamingCleanup::DeleteAllDrawableReferences()
{
	streamAssert(sysThreadType::IsUpdateThread()); // Don't even think about it.

	const fwLinkList<ActiveEntity>& activeList = g_SceneStreamerMgr.GetStreamingLists().GetActiveList();

	fwLink<ActiveEntity>* pLastLink = activeList.GetLast()->GetPrevious();

	while(pLastLink != activeList.GetFirst())
	{
		CEntity* pEntity = static_cast<CEntity*>(pLastLink->item.GetEntity());
		pLastLink = pLastLink->GetPrevious();

#if STREAMING_DETAILED_STATS
		CStreaming::GetCleanup().MarkDrawHandlerDeleted(pEntity, -2.0f);
#endif // STREAMING_DETAILED_STATS

		pEntity->DeleteDrawable();
	}
}

// Remove all references to a drawable.
bool CStreamingCleanup::PurgeDrawable(rmcDrawable* pDrawable)
{
	streamAssert(sysThreadType::IsUpdateThread()); // Don't even think about it.

	const fwLinkList<ActiveEntity>& activeList = g_SceneStreamerMgr.GetStreamingLists().GetActiveList();
	fwLink<ActiveEntity>* pLastLink = activeList.GetLast()->GetPrevious();

	while(pLastLink != activeList.GetFirst())
	{
		CEntity* pEntity = static_cast<CEntity*>(pLastLink->item.GetEntity());
		pLastLink = pLastLink->GetPrevious();

		if (pEntity->GetDrawable() == pDrawable)
		{
#if STREAMING_DETAILED_STATS
			CStreaming::GetCleanup().MarkDrawHandlerDeleted(pEntity, -3.0f);
#endif // STREAMING_DETAILED_STATS
			pEntity->DeleteDrawable();
			streamAssertf(!pEntity->GetIsTypeLight(), "The streaming cleanup deleted a light entity (%s) via purge.",
				pEntity->GetModelName());

			// remove object if 
			if(pEntity->GetBaseModelInfo()->GetNumRefs() == 0)
			{
				return true;
			}
		}
	}
	return true;
}


//
// name:		CStreamingCleanup::DeleteVehiclesAndPeds
// description:	Delete vehicles and peds to make space
bool CStreamingCleanup::DeleteVehiclesAndPeds(bool bCutscene)
{
	static bool bDeleteVehicle=false;
	s32 minVehicles = MIN_NUMBER_STREAMED_CARS;
	s32 minPeds = MIN_NUMBER_STREAMED_PEDS;
	bool bForce = false;

	if(bCutscene)
	{
		minVehicles = 2;
		minPeds = 2;
		bForce = true;
	}

	// if last time I deleted a vehicle lets delete a ped this time
	bDeleteVehicle = !bDeleteVehicle;
	if(bDeleteVehicle)
	{
		// if there is a vehicle to delete
		if(gPopStreaming.GetAppropriateLoadedCars().CountMembers() > minVehicles)
		{
			// remove vehicle
			if(gPopStreaming.StreamOneCarOut(bForce))
				return true;
		}
		// let function fall out to deleting peds if no vehicle was found
	}
	// if there is a ped to delete
	if(gPopStreaming.GetAppropriateLoadedPeds().CountMembers() > minPeds)
	{
		// remove ped
		if(gPopStreaming.StreamOnePedOut(bForce))
			return true;
	}
	// if there is a vehicle to delete
	if(gPopStreaming.GetAppropriateLoadedCars().CountMembers() > minVehicles)
	{
		// remove vehicle
		if(gPopStreaming.StreamOneCarOut(bForce))
			return true;
	}
	return false;
}

#if SUPPORT_STREAMING_SIM_MODE
bool CStreamingCleanup::SimulateRemovalNew(DeletionMap &deletionMap, StreamingEntry *entry, int &virtualMemoryToFree, int &physicalMemoryToFree)
{
/*	if (strStreamingEngine::GetInfo().IsObjectRequired(entry->m_Index))
	{
		// Can't.
		return;
	}*/

	if (entry->m_Parent)
	{
		entry->m_RealDepCount--;
	}

	if (entry->m_Status == STRINFO_LOADED)
	{
		entry->m_refCount++;

//		strStreamingInfo &info = strStreamingEngine::GetInfo().GetStreamingInfoRef(entry->m_Index);
//		streamDisplayf("SIMULATE REMOVAL - index %d %s, %d/%d, status=%s", entry->m_Index.Get(), strStreamingEngine::GetObjectName(entry->m_Index),
//			entry->m_refCount, entry->m_RealRefCount, strStreamingInfo::GetFriendlyStatusName(info.GetStatus()));
	}
	else
	{
		//streamDisplayf("Not Loaded - %s", strStreamingEngine::GetObjectName(entry->m_Index));
		//streamAssertf(entry->m_RealRefCount == 0, "%s has a ref count of %d even though status is %s",
		//	strStreamingEngine::GetObjectName(entry->m_Index), entry->m_RealRefCount, strStreamingInfo::GetFriendlyStatusName(entry->m_Status));
	}


	// There are more references to this object than its actual reference count?
	streamAssertf(entry->m_RealRefCount >= entry->m_refCount, "Mismatch during removal simulation - %s (status is %s) has a ref count of %d, but %d removal simulations have been done already.",
		strStreamingEngine::GetObjectName(entry->m_Index), strStreamingInfo::GetFriendlyStatusName(entry->m_Status),
		entry->m_RealRefCount, entry->m_refCount);

	// Would we be able to delete this object by now?
	if (entry->m_RealRefCount == entry->m_refCount)
	{
		streamAssert(entry->m_RealDepCount >= 0);
		if (entry->m_RealDepCount == 0)
		{
			// Yes.
			if (entry->m_Status == STRINFO_LOADED)
			{
				// Write this down as an object we can delete.
				// Put it on the deletion map even if it's not loaded yet - we have to make sure it'll be
				// unrequested in case it's on the request list.
				deletionMap.m_ObjectsToRemove.Append() = entry->m_Index;

				virtualMemoryToFree -= entry->m_virtualSize;
				physicalMemoryToFree -= entry->m_physicalSize;

				// If we get to delete this entry, we can delete its children as well.
				int count = entry->m_ReferenceCount;

				for (int x=0; x<count; x++)
				{
					entry->m_IsWorthDeleting |= SimulateRemovalNew(deletionMap, entry->m_References[x], virtualMemoryToFree, physicalMemoryToFree);
				}

	//			if (virtualMemoryToFree || physicalMemoryToFree)
	//			{
	//				entry->m_IsWorthDeleting = true;
	//			}

	/*			if (entry->m_IsWorthDeleting)
				{
					streamDisplayf("%s found worth deleting", strStreamingEngine::GetObjectName(entry->m_Index));
				}*/

				return true;
			}
		}
	}

	return false;
}
#endif // SUPPORT_STREAMING_SIM_MODE

bool CStreamingCleanup::PerformRemoval(strIndex index, u32 ignoreFlags, int &virtualMemoryToFree, int &physicalMemoryToFree, strIndex indexNeedingMemory, bool *mustAbort)
{
	strStreamingModule*	streamingModule = strStreamingEngine::GetInfo().GetModule(index);
	strLocalIndex localIndex = streamingModule->GetObjectIndex(index);

	if (index == indexNeedingMemory)
	{
		*mustAbort = true;
		return false;
	}

	if (strStreamingEngine::GetInfo().IsObjectReadyToDelete(index, ignoreFlags))
	{
		strStreamingInfo &info = strStreamingEngine::GetInfo().GetStreamingInfoRef(index);

		//if ((info.GetFlags() & ignoreFlags) == 0)
		{
			if (info.GetStatus() == STRINFO_LOADING || info.GetStatus() == STRINFO_LOADED)
			{
				virtualMemoryToFree += info.ComputeOccupiedVirtualSize(index, false);
				physicalMemoryToFree += info.ComputePhysicalSize(index, false);
			}

			STR_TELEMETRY_MESSAGE_LOG("(streaming/memory/cleanup)Freeing up object %s (savings: %dKB virt, %dKB phys)", strStreamingEngine::GetObjectName(index),
				virtualMemoryToFree / 1024, physicalMemoryToFree / 1024);

			strStreamingEngine::GetInfo().RemoveObject(index);

			// We can start deleting the children.
			strIndex deps[ STREAMING_MAX_DEPENDENCIES ];
			int depCount = streamingModule->GetDependencies( localIndex, deps, STREAMING_MAX_DEPENDENCIES);

			for (int j = 0; j < depCount; ++j)
			{
				PerformRemoval(deps[j], ignoreFlags, virtualMemoryToFree, physicalMemoryToFree, indexNeedingMemory, mustAbort);
			}
		}
	}

	return true;
}

#if SUPPORT_STREAMING_SIM_MODE

StreamingEntry* CStreamingCleanup::AddCandidateNew(DeletionMap &deletionMap, BucketEntry *bucketEntry, strIndex index, u32 ignoreFlags, StreamingEntry *parent, int &virtualMemoryToFree, int &physicalMemoryToFree, strIndex indexNeedingMemory, bool *mustAbort)
{
	streamAssert(index.IsValid());

	if (index == indexNeedingMemory)
	{
		// We're trying to free up memory for something that isn't that important. Abort.
		*mustAbort = true;
		strStreamingEngine::GetInfo().RemoveObject(indexNeedingMemory);
		return NULL;
	}

	// Did we already create an entry for this streamable?
	StreamingEntry*		entry = deletionMap.m_StreamableMap.Access(index);

	if (!entry)
	{
		// No - let's create a new one.
		strStreamingInfo*	streamingInfo = strStreamingEngine::GetInfo().GetStreamingInfo( index );

		// Unless it has one of the ignore flags.
		if ( streamingInfo->IsFlagSet( ignoreFlags ) )
			return NULL;

		strIndex deps[ STREAMING_MAX_DEPENDENCIES ];

		entry = &deletionMap.m_StreamableMap.Insert(index).data;
		entry->m_Status = streamingInfo->GetStatus();

		if (entry->m_Status == STRINFO_LOADED && (!(streamingInfo->GetFlags() & STRFLAG_INTERNAL_DUMMY)))
		{
			entry->m_virtualSize = streamingInfo->ComputeOccupiedVirtualSize(index);
			entry->m_physicalSize = streamingInfo->ComputePhysicalSize(index);
		}
		else
		{
			entry->m_virtualSize = 0;
			entry->m_physicalSize = 0;
		}

		entry->m_refCount = 0;
		entry->m_Parent = parent;
		entry->m_Index = index;

#if ONE_STREAMING_HEAP
		entry->m_physicalSize += entry->m_virtualSize;
		entry->m_virtualSize = 0;
#endif // ONE_STREAMING_HEAP

		strStreamingModule*	streamingModule = strStreamingEngine::GetInfo().GetModule(index);
		int localIndex = streamingModule->GetObjectIndex(index);

		entry->m_RealRefCount = (short) streamingModule->GetNumRefs(localIndex);
		entry->m_RealDepCount = (short) streamingInfo->GetDependentCount();
		entry->m_IsWorthDeleting = false;

		int depCount = streamingModule->GetDependencies( localIndex, deps, STREAMING_MAX_DEPENDENCIES);

		if (streamVerifyf(deletionMap.m_ReferenceBufferCursor + depCount <= DeletionMap::REF_BUFFER_SIZE, "Too many references! Increase REF_BUFFER_SIZE."))
		{
			StreamingEntry **references = &deletionMap.m_ReferenceBuffer[deletionMap.m_ReferenceBufferCursor];
			deletionMap.m_ReferenceBufferCursor += depCount;
			entry->m_References = references;
			entry->m_ReferenceCount = (char) depCount;

			for (int j = 0; j < depCount; ++j)
			{
				StreamingEntry *childEntry = AddCandidateNew(deletionMap, NULL, deps[j], ignoreFlags /* TODO: Or 0? */, entry, virtualMemoryToFree, physicalMemoryToFree, indexNeedingMemory, mustAbort);

				if (childEntry)
				{
					*(references++) = childEntry;
				}
				else
				{
					entry->m_ReferenceCount--;
				}
			}

			// Short-circuit if we determine that this allocation is not worth it.
			/*if (*mustAbort)
			{
				return NULL;
			}*/
		}
	}

	// Short-circuit if we determine that this allocation is not worth it.
	if (!(*mustAbort))
	{
		if (bucketEntry)
		{
			deletionMap.m_EntitiesToDelete.Append() = bucketEntry;
		}

		// If this is an archetype, see what would happen if we were to delete it.
		if (parent == NULL)
		{
	/////		streamDisplayf("Simulate removal on %s", strStreamingEngine::GetObjectName(entry->m_Index));
			SimulateRemovalNew(deletionMap, entry, virtualMemoryToFree, physicalMemoryToFree);
		}
	}

	return entry;
}
#endif // SUPPORT_STREAMING_SIM_MODE


bool CStreamingCleanup::MakeSpaceForResourceFile(const char* filename, const char* ext, s32 version)
{
	char file[RAGE_MAX_PATH];
	pgRscBuilder::ConstructName(file, RAGE_MAX_PATH, filename, ext);
	const fiDevice* pDevice = fiDevice::GetDevice(file, true);

	streamAssertf(pDevice, "Could not get device for %s", file);

	datResourceInfo info;
	if (pDevice)
	{	
		const int resVersion = pDevice->GetResourceInfo(file, info);
		if (resVersion == version) 
		{
			datResourceMap map;
			pgRscBuilder::GenerateMap(info, map);

			MakeSpaceForMap(map, strIndex(), false);
		}
		
		streamAssertf(resVersion || resVersion == version, "Could not get resource info for file '%s' ", file);
		streamAssertf(!resVersion || resVersion == version, "File '%s' is version %d, expecting %d", file, resVersion, version);
	}

	return true;
}

CStreamingCleanup::RecurseResult CStreamingCleanup::RecurseDeleteDependencies(strIndex index, strIndex indexNeedingMemory, u32 ignoreFlags)
{
	if (index == indexNeedingMemory)
	{
		streamErrorf("Trying to free up memory for %s - not important enough to warrant that.", strStreamingEngine::GetInfo().GetObjectName(index));
		return MUST_ABORT;
	}

	strStreamingInfo &info = strStreamingEngine::GetInfo().GetStreamingInfoRef(index);

	if (info.GetFlags() & ignoreFlags)
	{
		return NOTHING_DELETED;
	}

	if (/*1 ||*/ info.GetStatus() != STRINFO_NOTLOADED && info.GetDependentCount() == 0)
	{
		strStreamingModule*	streamingModule = strStreamingEngine::GetInfo().GetModule(index);
		strLocalIndex localIndex = streamingModule->GetObjectIndex(index);

		if (streamingModule->GetNumRefs(localIndex) == 0)
		{
//////////			streamDisplayf("Removing %s, status is %s", strStreamingEngine::GetInfo().GetObjectName(index),
///////////				strStreamingInfo::GetFriendlyStatusName(strStreamingEngine::GetInfo().GetStreamingInfoRef(index).GetStatus()));

			u32 memory = 0;
			if (info.GetStatus() == STRINFO_LOADED)
			{
				memory = info.ComputeOccupiedVirtualSize(index, false) + info.ComputePhysicalSize(index, false);
			}

			RecurseResult result = (memory > 0) ? FREED_MEMORY : DELETED_DUMMIES;

			STR_TELEMETRY_MESSAGE_LOG("(streaming/memory/cleanup)Freeing up object %s", strStreamingEngine::GetObjectName(index));

			strStreamingEngine::GetInfo().RemoveObject(index);

			strIndex deps[ STREAMING_MAX_DEPENDENCIES ];
			int depCount = streamingModule->GetDependencies(localIndex, deps, STREAMING_MAX_DEPENDENCIES);

			for (int j = 0; j < depCount; ++j)
			{
				// Is this now available for deletion?
				RecurseResult subResult = RecurseDeleteDependencies(deps[j], indexNeedingMemory, ignoreFlags);
				if (result == MUST_ABORT)
				{
					return subResult;
				}

				if (subResult == FREED_MEMORY)
				{
					result = FREED_MEMORY;
				}
			}

			return result;
		}
		else
		{
/////////			streamDisplayf("Ref count %d, dep count %d on %s", streamingModule->GetNumRefs(localIndex),
////////				info.GetDependentCount(), strStreamingEngine::GetInfo().GetObjectName(index));
		}
	}
	else
	{
////////		streamDisplayf("Cannot delete %s - status=%s, dependents (%d)", strStreamingEngine::GetInfo().GetObjectName(index),
////////			strStreamingInfo::GetFriendlyStatusName(info.GetStatus()), info.GetDependentCount());
	}

	return NOTHING_DELETED;
}


CStreamingCleanup::RecurseResult CStreamingCleanup::DeleteEntityAndDependencies(DeletionMap & /*deletionMap*/, BucketEntry *bucketEntry, CEntity *entity, strIndex indexNeedingMemory, u32 ignoreFlags, CStreamingBucketSet &BANK_ONLY(bucketSet))
{
	if (!IsSceneStreamingNoSim() && !entity->GetDrawHandlerPtr())
	{
		return NOTHING_DELETED;
	}

	strStreamingModule* streamingModule = fwArchetypeManager::GetStreamingModule();
	Assertf((entity->GetOwnedBy() != ENTITY_OWNEDBY_STATICBOUNDS), "attempting to delete dummy entity created by static bounds! Crash.");

	u32 objIndex = entity->GetModelIndex();

	// Sanity check - we've had problems with invalid objIndices.
/*
	if (objIndex >= streamingModule->GetCount())
	{
		streamErrorf("Bad object index detected - %d", objIndex);
		streamErrorf("Number of registered archetypes: %d", streamingModule->GetCount());
		streamErrorf("Entity in question: %p, entity index: %d", entity, modelIndex);
		streamErrorf("Model name: %s", entity->GetModelName());
		streamErrorf("Archetype/BaseModelInfo ptr from entity: %p", entity->GetBaseModelInfo());
		streamErrorf("Archetype stream slot: %d", entity->GetBaseModelInfo()->GetStreamSlot());
		streamErrorf("Base model pointer from model ID: %p", CModelInfo::GetBaseModelInfo(modelId));
		streamErrorf("Names of both base models: %s (from entity ptr), %s (from model ID)",
			entity->GetBaseModelInfo()->GetModelName(), CModelInfo::GetBaseModelInfo(modelId)->GetModelName());

		streamAssertf(false, "Bad model index (%d) - something is broken with the entity and will likely cause more problems. See TTY for more information.", objIndex);
		return NOTHING_DELETED;
	}
#if __DEV || __BANK
	if (streamingModule == fwArchetypeManager::GetStreamingModule())
	{
		fwArchetype* pModelInfo = fwArchetypeManager::GetArchetype(objIndex);

		if (pModelInfo == NULL)
		{
			Errorf("Invalid archetype being deleted (%d) - this will crash!", objIndex);
			Vec3V pos = entity->GetTransform().GetPosition();
			Errorf("Type: %d", entity->GetType());
			Errorf("Position: %.2f %.2f %.2f", pos.GetXf(), pos.GetYf(), pos.GetZf());
			Errorf("Entity: %p; dmap: %p; bentry: %p", entity, &deletionMap, bucketEntry);
			Assertf(false, "DELETING INVALID ENTITY - see TTY for details.");
		}
	}
#endif // __DEV || __BANK
*/
	strIndex archIndex = streamingModule->GetStreamingIndex(strLocalIndex(objIndex));

	if (strStreamingEngine::GetInfo().GetStreamingInfoRef(archIndex).GetFlags() & ignoreFlags)
	{
////////		streamDebugf1("Ignore flags hit (%x/%x) on %s, entity %p, objIndex %d, strIndex %d", strStreamingEngine::GetInfo().GetStreamingInfoRef(archIndex).GetFlags(), ignoreFlags, entity->GetModelName(), entity, objIndex, archIndex.Get());
		return NOTHING_DELETED;
	}

	// Let's see if we can delete this guy.
	s32 virtMemoryFree = 0, physMemoryFree = 0;

	bool mustAbort = false;

	if (IsSceneStreamingNoSim())
	{
		STRVIS_SET_SCORE(bucketEntry->m_Score);
		streamDebugf3("Deleting entity %s, score %f", entity->GetModelName(), bucketEntry->m_Score);
#if STREAMING_DETAILED_STATS
		MarkDrawHandlerDeleted(entity, bucketEntry->m_Score);
#endif // STREAMING_DETAILED_STATS
		// Let's be a MAN and delete this thing right away.
		entity->DeleteDrawable();

		streamAssertf(!entity->GetIsTypeLight(), "The streaming cleanup deleted a light entity (%s). That shouldn't have been in the cleanup bucket in the first place",
			entity->GetModelName());

#if __BANK
		bucketSet.TrackDeletion((int) bucketEntry->m_Score);
		if (CSceneStreamerDebug::DrawRequestsOnVmap()) { fwVectorMap::Draw3DBoundingBoxOnVMap(entity->GetBoundBox(), Color_yellow); }
#endif // __BANK

#if STREAMING_TELEMETRY_INTERFACE
		if (strStreamingEngine::IsTelemetryLogEnabled()) {
			Vec3V entityCenter;
			float entityRadius;
			entityRadius = entity->GetBoundCentreAndRadius(RC_VECTOR3(entityCenter));

			Vec3V camPos = RCC_VEC3V(g_SceneToGBufferPhaseNew->GetCameraPositionForFade());

			float dist = Dist(entityCenter, camPos).Getf();
	
			TELEMETRY_MESSAGE_LOG("(streaming/memory/cleanup)Freeing up object %s, score=%f, dist=%f, radius=%f", strStreamingEngine::GetObjectName(archIndex), bucketEntry->m_Score, dist, entityRadius);
		}
#endif // STREAMING_TELEMETRY_INTERFACE

		PerformRemoval(archIndex, ignoreFlags, virtMemoryFree, physMemoryFree, indexNeedingMemory, &mustAbort);
		STRVIS_RESET_SCORE();
	}
#if SUPPORT_STREAMING_SIM_MODE
	else
	{
		AddCandidateNew(deletionMap, bucketEntry, archIndex, ignoreFlags, NULL, virtMemoryFree, physMemoryFree, indexNeedingMemory, &mustAbort);
	}
#endif // SUPPORT_STREAMING_SIM_MODE

	if (mustAbort)
	{
		streamDebugf2("Giving up allocating memory for %s - deemed not important enough. (When trying to free up %s)", strStreamingEngine::GetObjectName(indexNeedingMemory), entity->GetModelName());

		LogChurnProtectionEvent(*bucketEntry);

		return MUST_ABORT;
	}

	return (virtMemoryFree != 0 || physMemoryFree != 0) ? FREED_MEMORY : NOTHING_DELETED;
}

void CStreamingCleanup::LogChurnProtectionEvent(BucketEntry &bucketEntry)
{
	CEntity *entity = bucketEntry.GetEntity();

	STR_TELEMETRY_MESSAGE_WARNING("(streaming/memory/cleanup)Giving up on freeing memory - score is %f", bucketEntry.m_Score);

	streamDebugf1("Aborting allocation for %s (score=%f)", entity->GetModelName(), bucketEntry.m_Score);

#if STREAMING_DETAILED_STATS
	MarkDrawHandlerDeleted(entity, bucketEntry.m_Score);
#endif // STREAMING_DETAILED_STATS
	entity->DeleteDrawable();

	streamAssertf(!entity->GetIsTypeLight(), "The streaming cleanup deleted a light entity (%s). That shouldn't have been in the cleanup bucket in the first place",
		entity->GetModelName());

	g_SceneStreamerMgr.GetStreamingLists().MarkCutoffEvent(bucketEntry.m_Score);

#if __BANK
	g_SceneStreamerMgr.GetStreamingLists().TrackCutoff(bucketEntry.m_Score);
	u32 timestamp = TIME.GetFrameCount();
	if (m_LastChurnTimestamp == timestamp)
	{
		// Multiple churns in one frame - pick the worst one.
		m_LastChurnPoint = Max(m_LastChurnPoint, bucketEntry.m_Score);
	}
	else
	{
		m_LastChurnTimestamp = timestamp;
		m_LastChurnPoint = bucketEntry.m_Score;
	}
#endif // __BANK
}

CStreamingCleanup::CleanupResult CStreamingCleanup::FindLowestScoringEntityToDelete(strIndex indexNeedingMemory, u32 ignoreFlags)
{
	PF_FUNC(FindLowestScoring);

	// We're not allowed to free up memory for another file on auxiliary threads.
	streamAssert(indexNeedingMemory.IsInvalid() || sysThreadType::IsUpdateThread());

	CStreamingLists &lists = g_SceneStreamerMgr.GetStreamingLists();
	CStreamingBucketSet &activeList = lists.GetActiveEntityList().GetBucketSet();
	CStreamingBucketSet &neededList = lists.GetNeededEntityList().GetBucketSet();

	DeletionMap &deletionMap = s_DeletionMap;

	// If we're starting a new frame, begin from scratch - don't continue where we left off last time.
	if (deletionMap.m_Timestamp != lists.GetTimestamp())
	{
		// The buckets have changed. Reset the map.
		deletionMap.Reset();
		deletionMap.m_Timestamp = lists.GetTimestamp();
	}

	STR_TELEMETRY_MESSAGE_LOG("(streaming/memory/cleanup)Trying to remove entities to make room for %d (%s), ignoreFlags=%x", indexNeedingMemory.Get(), (indexNeedingMemory.IsValid()) ? strStreamingEngine::GetObjectName(indexNeedingMemory) : "", ignoreFlags);

	// If this is a required entity, don't use the churn protection - we HAVE to stream it in.
	// Note that IsObjectRequired doesn't check for LOADSCENE, hence the manual check.
//	if (indexNeedingMemory.IsValid() && strStreamingEngine::GetInfo().IsObjectRequired(indexNeedingMemory))
	if (indexNeedingMemory.IsValid() && (strStreamingEngine::GetInfo().GetStreamingInfoRef(indexNeedingMemory).GetFlags() & (ignoreFlags | STRFLAG_LOADSCENE | STRFLAG_DONTDELETE)))
	{
		// We HAVE to get this one through.
		indexNeedingMemory = strIndex();
	}

#if __BANK
	// Don't do churn protection if it's disabled.
	if (m_DisableChurnProtection)
	{
		indexNeedingMemory = strIndex();
	}
#endif // __BANK

	// Let's get the top contenders from the active and the needed list.
	int actIndex = s_DeletionMap.m_ActiveIndex;
	int needIndex = s_DeletionMap.m_NeededIndex;

	int actCount = activeList.GetEntryCount();
	int needCount = neededList.GetEntryCount();

	BucketEntry *actPtr = activeList.GetEntries();
	BucketEntry *needPtr = neededList.GetEntries();

	// Make sure the SPU is done sorting the lists.
	activeList.WaitForSorting();
	neededList.WaitForSorting();

	CleanupResult resultState = CLEANUP_FAILED;

	// Let's keep going until we've exhausted both lists.
	while (actIndex < actCount || needIndex < needCount)
	{
		BucketEntry *activeBucketEntry = NULL;
		BucketEntry *neededBucketEntry = NULL;

		CEntity *topActiveContender = NULL;
		
		// Find the next valid contender in the active list.
		while (actIndex < actCount)
		{
			activeBucketEntry = &actPtr[actIndex];
			topActiveContender = activeBucketEntry->GetValidatedEntity();

			// Skip empty entries.
			if (topActiveContender)
			{
				break;
			}

			actIndex++;
		}

		// Get the next valid contender in the needed list.
		CEntity *topNeededContender = NULL;

		while (needIndex < needCount)
		{
			neededBucketEntry  = &needPtr[needIndex];
			topNeededContender = neededBucketEntry->GetValidatedEntity();

			// Skip empty entries.
			if (topNeededContender)
			{
				break;
			}

			needIndex++;
		}

		// This could happen if there were nothing but invalid entries in both
		// lists. In that case, we don't have anything else to clean up.
		if(!topActiveContender && !topNeededContender)
		{
			break;
		}

		// Let's figure out who's got the lower score. Lower scores gets deleted.
		bool activeTrumpsNeeded;

		// Nothing in the active list means we'll take the needed contender.
		if (!topActiveContender)
		{
			activeTrumpsNeeded = false;
		}
		else
		{
			// And nothing in the needed list means we'll take the active one.
			if (!topNeededContender)
			{
				activeTrumpsNeeded = true;
			}
			else
			{
				// We have two contenders - let them duke it out.
				activeTrumpsNeeded = (activeBucketEntry->m_Score < neededBucketEntry->m_Score);
			}
		}

		if (activeTrumpsNeeded)
		{
			actIndex++;

			// An active element we no longer need. Kill.
			RecurseResult result = DeleteEntityAndDependencies(deletionMap, activeBucketEntry, topActiveContender, indexNeedingMemory, ignoreFlags, activeList);

			if (result == MUST_ABORT)
			{
				// We have to stop now.
				resultState = CLEANUP_ABORTED;
				break;
			}

			if (result == FREED_MEMORY)
			{
				// We freed up memory. Let's try allocating the map again.
#if SUPPORT_STREAMING_SIM_MODE
				deletionMap.ExecuteDeletion();
#endif // SUPPORT_STREAMING_SIM_MODE
				resultState = CLEANUP_SUCCESS;
				break;
			}
		}
		else
		{
			needIndex++;

			// A needed element is not really that needed after all. Remove the request for it.
			strStreamingModule* streamingModule = fwArchetypeManager::GetStreamingModule();
			u32 objIndex = topNeededContender->GetModelIndex();

			strIndex archIndex = streamingModule->GetStreamingIndex(strLocalIndex(objIndex));

			if (indexNeedingMemory == archIndex)
			{
				streamWarningf("Request for %s deemed not important enough - removing.", strStreamingEngine::GetInfo().GetObjectName(indexNeedingMemory));
				LogChurnProtectionEvent(*neededBucketEntry);
				strStreamingEngine::GetInfo().RemoveObject(archIndex);
				resultState = CLEANUP_ABORTED;
				break;
			}

			RecurseResult result = DeleteEntityAndDependencies(deletionMap, neededBucketEntry, topNeededContender, indexNeedingMemory, ignoreFlags, neededList);

			if (result == MUST_ABORT)
			{
				// We have to stop now.
				resultState = CLEANUP_ABORTED;
				break;
			}

			if (result == FREED_MEMORY)
			{
				// We freed up memory. Let's try allocating the map again.
#if SUPPORT_STREAMING_SIM_MODE
				deletionMap.ExecuteDeletion();
#endif // SUPPORT_STREAMING_SIM_MODE
				resultState = CLEANUP_SUCCESS;
				break;
			}
		}
	}

	deletionMap.m_NeededIndex = needIndex;
	deletionMap.m_ActiveIndex = actIndex;

	m_LastAllocationWasAborted = (resultState == CLEANUP_ABORTED);

	return resultState;
}

int CStreamingCleanup::DeleteUnusedEntities(int maxObjsToDelete, bool ASSERT_ONLY(mustDelete), float scoreCutoff)
{
	STRVIS_AUTO_CONTEXT(strStreamingVisualize::GARBAGECOLLECT);

	ASSERT_ONLY(int deletedEntities = 0);

	// We're not allowed to free up memory for another file on auxiliary threads.
	streamAssert(sysThreadType::IsUpdateThread());

	CStreamingLists &lists = g_SceneStreamerMgr.GetStreamingLists();
	CStreamingBucketSet &activeList = lists.GetActiveEntityList().GetBucketSet();

	DeletionMap &deletionMap = s_DeletionMap;
	u32 ignoreFlags = STR_DONTDELETEARCHETYPE_MASK | STRFLAG_LOADSCENE;

	if (deletionMap.m_Timestamp != lists.GetTimestamp())
	{
		// The buckets have changed. Reset the map.
		deletionMap.Reset();
		deletionMap.m_Timestamp = lists.GetTimestamp();
	}

	int actIndex = s_DeletionMap.m_ActiveIndex;
	int startIndex = actIndex;
	int actCount = Min(activeList.GetEntryCount(), actIndex + maxObjsToDelete);

	BucketEntry *actPtr = activeList.GetEntries();

	activeList.WaitForSorting();

	OUTPUT_ONLY(float lastScore = -1.0f;)

	// Note that the list is considered unsorted at this point, so we'll have to go through it in its entirety.
	while (actIndex < actCount)
	{
		BucketEntry *activeBucketEntry = NULL;
		activeBucketEntry = &actPtr[actIndex];

		OUTPUT_ONLY(lastScore = activeBucketEntry->m_Score;)

		if (activeBucketEntry->m_Score > scoreCutoff)
		{
			streamAssertf(deletedEntities || !mustDelete, "Unable to delete any entities - the lowest score I found was %f.", activeBucketEntry->m_Score);
			break;
		}

		if (activeBucketEntry->IsStillValid())
		{
			DeleteEntityAndDependencies(deletionMap, activeBucketEntry, activeBucketEntry->GetEntity(), strIndex(), ignoreFlags, activeList);
		}
		
		actIndex++;

		ASSERT_ONLY(deletedEntities++);
	}

	if (actIndex != startIndex)
	{
		streamDebugf2("Clean-up done - got to %d out of %d, last score is %f.", actIndex, actCount, lastScore);

#if SUPPORT_STREAMING_SIM_MODE
		deletionMap.ExecuteDeletion();
#endif // SUPPORT_STREAMING_SIM_MODE
		deletionMap.m_ActiveIndex = actIndex;
	}
	
	return actIndex - startIndex;
}

bool g_HACK_GlobalSRLSkipGC;
bool g_HACK_GlobalSRLForceGC;

void CStreamingCleanup::RemoveExcessEntities()
{
	CStreamingLists &lists = g_SceneStreamerMgr.GetStreamingLists();
	CStreamingBucketSet &activeBucketSet = lists.GetActiveEntityList().GetBucketSet();

#if RSG_DURANGO || RSG_ORBIS || __WIN32PC
	const bool skipGC = true;
#else
	const bool skipGC = g_HACK_GlobalSRLSkipGC || (CFocusEntityMgr::GetMgr().IsPlayerPed() && g_ContinuityMgr.GetPlayerVelocity()>20.0f);
#endif
	int oldCount = activeBucketSet.GetEntryCount();
	bool performCleanup = s_ForceAutoCleanup || (!skipGC && (oldCount >= s_CurrentThreshold));
	bool emergency = false;
	float cutoff = s_AutoCleanupCutoff;

	// We should perform garbage collection if memory is low.
	size_t virtUsed = strStreamingEngine::GetAllocator().GetVirtualMemoryAvailable()>=100? strStreamingEngine::GetAllocator().GetVirtualMemoryUsed() / (strStreamingEngine::GetAllocator().GetVirtualMemoryAvailable()/100) : 0;
	size_t physUsed = strStreamingEngine::GetAllocator().GetPhysicalMemoryAvailable()>=100? strStreamingEngine::GetAllocator().GetPhysicalMemoryUsed() / (strStreamingEngine::GetAllocator().GetPhysicalMemoryAvailable()/100) : 0;
	(void)virtUsed;(void)physUsed;	// keep Durango compiler happy

	int maxToCleanUp = s_MaxObjsToAutoCleanup;

	if ( !skipGC && (g_HACK_GlobalSRLForceGC || (strStreamingAllocator::GetSloshState() && (virtUsed >= s_MemoryGarbageCollectionThresholdVirt || physUsed >= s_MemoryGarbageCollectionThresholdPhys))))
	{
		streamDebugf2("Memory threshold hit (%" SIZETFMT "d%% virt, %" SIZETFMT "d%% phys) - triggering garbage collection", virtUsed, physUsed);
		if (!s_GarbageCollectEntitiesOnly)
		{
			strStreamingAllocator::SetSloshState(strStreamingEngine::GetInfo().DeleteNextLeastUsedObject(STRFLAG_LOADSCENE, NULL));
			strPackfileManager::DeleteLRUPackfile();
		}
		else
		{
			strStreamingAllocator::SetSloshState(false);
		}
		
		performCleanup = true;
		cutoff = s_GarbageCollectionCutoff;
		maxToCleanUp = 2;
		g_HACK_GlobalSRLForceGC = false; 
	}

	int maxLoadedInfo = fwConfigManager::GetInstance().GetSizeOfPool(ATSTRINGHASH("MaxLoadedInfo",0xF2E37BE0), CONFIGURED_FROM_FILE);
	if (strStreamingEngine::GetInfo().GetLoadedInfoPool().GetUsedCount() > maxLoadedInfo - 100)
	{
		Displayf("Emergency loaded object cleanup");
		performCleanup = true;
		emergency = true;
		cutoff = 6.0f;
	}

	if (oldCount >= OBJ_INSTANCES_LIMIT)
	{
		CStreaming::GetCleanup().AssertAndDumpEntities();
		performCleanup = true;
		emergency = true;
		cutoff = 6.0f;
	}

	if (performCleanup)
	{
		sysTimer timer;
		int cleanedObjs = CStreaming::GetCleanup().DeleteUnusedEntities((emergency) ? (OBJ_INSTANCES_LIMIT - s_CurrentThreshold) : maxToCleanUp, emergency, cutoff);

		if (cleanedObjs)
		{
			streamDebugf2("Performed entity auto-cleanup because current count is %d, cleaned up %d entities, only took %.2fms", oldCount, cleanedObjs, timer.GetMsTime());
		}

		// Do we have more left that we can delete? If so, do it again next frame.
		s_ForceAutoCleanup = (cleanedObjs == s_MaxObjsToAutoCleanup);
	}
}


void CStreamingCleanup::AssertAndDumpEntities()
{
#if !__NO_OUTPUT
	static bool alreadyDumped;

	if (!alreadyDumped)
	{
		// Let's not spew the user to death.
		alreadyDumped = true;
		streamDisplayf("List of all active entities:");
		streamDisplayf("Entity\tModel\tPos X\tPos Y\tPos Z\tType\tUnrendered frame count");

		CStreamingLists &lists = g_SceneStreamerMgr.GetStreamingLists();

		const fwLinkList<ActiveEntity>& activeLinkedList = lists.GetActiveEntityList().GetDrawableLinkList();
		fwLink<ActiveEntity>* pLink = activeLinkedList.GetFirst()->GetNext();

		while (pLink != activeLinkedList.GetLast())
		{
			CEntity* entity = static_cast<CEntity*>(pLink->item.GetEntity());
			u32 timeSinceLastVisible = pLink->item.GetTimeSinceLastVisible();
			pLink = pLink->GetNext();
			streamAssert(pLink != NULL);
			PrefetchDC(pLink);

			Vec3V pos = entity->GetTransform().GetPosition();

			streamDisplayf("%p\t%s\t%.2f\t%.2f\t%.2f\t%d",
				entity, entity->GetModelName(), pos.GetXf(), pos.GetYf(), pos.GetZf(),
				timeSinceLastVisible);
		}

		g_SceneStreamerMgr.DumpStreamingInterests();
	}
#endif // !__NO_OUTPUT

	streamWarningf("There are too many active entities. A complete list has been dumped to the TTY (it's LONG). Please open a bug and be sure to include the TTY.");
}


bool CStreamingCleanup::WasLastAllocationAborted() const
{
	// This function is only intended to be used on the update thread, where allocations can fail
	// due to an object being not important enough to warrant freeing up memory.
	streamAssert(sysThreadType::IsUpdateThread());

	return m_LastAllocationWasAborted;
}

bool CStreamingCleanup::WasLastAllocationFragmented() const
{
	// This function is only intended to be used on the update thread, where we allocate
	// resources.
	streamAssert(sysThreadType::IsUpdateThread());

	return m_LastAllocationWasFragmented;
}

#if RSG_PC
size_t CStreamingCleanup::GetPhysicalSize(datResourceMap &map)
{
	size_t physicalSize = 0; //virtualSize + physicalSize;

	// Vertex/Index buffers are now in virtual memory so counting both as physical.  
	// Better to be safe with an over estimation
	for (size_t i=0 /*map.VirtualCount*/; i<(size_t)(map.VirtualCount+map.PhysicalCount); ++i)
		physicalSize += map.Chunks[i].Size;

	return physicalSize;
}

size_t CStreamingCleanup::GetVirtualSize(datResourceMap &map)
{
	size_t physicalSize = 0; //virtualSize + physicalSize;

	// Vertex/Index buffers are now in virtual memory so counting both as physical.  
	// Better to be safe with an over estimation
	for (size_t i=0 /*map.VirtualCount*/; i<(size_t)(map.VirtualCount); ++i)
		physicalSize += map.Chunks[i].Size;

	return physicalSize;
}

bool CStreamingCleanup::HandleOutOfMemory(size_t virtualSize, size_t physicalSize)
{
#if USE_RESOURCE_CACHE
	//size_t streamSize = virtualSize + physicalSize;
	s64 iAvailable = grcResourceCache::GetInstance().GetStreamingMemory();
	streamDebugf1("Available Virt %" I64FMT "d Phys %" I64FMT "d VidMem %" I64FMT "d but Requires Virt %" SIZETFMT "u Phys %" SIZETFMT "d", 
		strStreamingEngine::GetAllocator().GetVirtualMemoryAvailable(), 
		strStreamingEngine::GetAllocator().GetPhysicalMemoryAvailable(),
		iAvailable, 
		virtualSize,
		physicalSize);

	if (virtualSize > strStreamingEngine::GetAllocator().GetVirtualMemoryAvailable())
	{
		// Out of Virtual Memory to stream in resources
	}
	else if (physicalSize > strStreamingEngine::GetAllocator().GetPhysicalMemoryAvailable())
	{
		// Out of Physical Memory to stream in temp resources
		Assertf(0, "Physical memory fragmented - Should only be used for temporary allocation");
		Warningf("Available Virt %" I64FMT "d Phys %" I64FMT "d VidMem %" I64FMT "d but Requires Virt %" SIZETFMT "u Phys %" SIZETFMT "d", 
			strStreamingEngine::GetAllocator().GetVirtualMemoryAvailable(), 
			strStreamingEngine::GetAllocator().GetPhysicalMemoryAvailable(),
			iAvailable, 
			virtualSize,
			physicalSize);
	}
	else if (iAvailable > (s64) physicalSize)
	{
		return true;
	}
	else if (!PARAM_disableExtraMem.Get()) // (iAvailable < (s64)physicalSize)
	{
		s64 iIncreaseAmount = (s64)(Min((u32)physicalSize, (u32)(16*1024*1024)) * 2.0f / grcResourceCache::GetInstance().GetPercentageForStreaming());
		streamDebugf1("Increasing Video Memory %" I64FMT "d Requested %" SIZETFMT "u", iIncreaseAmount, physicalSize);
		iIncreaseAmount += grcResourceCache::GetInstance().GetExtraMemory(MT_Video);
		grcResourceCache::GetInstance().SetExtraMemory(MT_Video, iIncreaseAmount);
		return true;
	}
#else
	virtualSize;
	physicalSize;
#endif // USE_RESOURCE_CACHE
	return false;
}
#endif // RSG_PC

bool CStreamingCleanup::HasSpaceFor(strStreamingAllocator& 
#if __CONSOLE || !USE_RESOURCE_CACHE
									allocator
#endif // __CONSOLE || !USE_RESOURCE_CACHE
									, size_t virtualSize
									, size_t physicalSize)
{
#if USE_RESOURCE_CACHE && RESOURCE_CACHE_MANAGING
	if (grcResourceCache::ManageResources())
	{
		s64 iAvailable = grcResourceCache::GetInstance().GetStreamingMemory();
		if (iAvailable < (s64)(/*virtualSize +*/ physicalSize))
			return false;
	}
#endif

#if __CONSOLE || !USE_RESOURCE_CACHE
	#if ONE_STREAMING_HEAP
		bool enoughMemory = allocator.GetPhysicalMemoryUsed() <= allocator.GetPhysicalMemoryAvailable() - virtualSize - physicalSize;
	#else // ONE_STREAMING_HEAP
		bool enoughMemory = ((!virtualSize) || (allocator.GetVirtualMemoryUsed() <= allocator.GetVirtualMemoryAvailable() - virtualSize)) &&
							((!physicalSize) || (allocator.GetPhysicalMemoryUsed() <= allocator.GetPhysicalMemoryAvailable() - physicalSize));
	#endif // ONE_STREAMING_HEAP
	return enoughMemory;
#else // !__CONSOLE
	#if ONE_STREAMING_HEAP
		physicalSize += virtualSize;
	#endif // #if ONE_STREAMING_HEAP	

	sysMemAllocator* pAllocator = NULL;
	pAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_VIRTUAL);
	if (pAllocator->GetMemoryAvailable() < virtualSize)
		return false;

	/* No need has proper safety
	if (pAllocator->GetLargestAvailableBlock() < virtualSize)
		return false;
	*/

	pAllocator = sysMemAllocator::GetMaster().GetAllocator(MEMTYPE_RESOURCE_PHYSICAL);
	if (pAllocator->GetMemoryAvailable() < physicalSize)
		return false;

	/* No need has proper safety
	if (pAllocator->GetLargestAvailableBlock() < physicalSize)
		return false;
	*/
	return true;
#endif // __CONSOLE

}

ASSERT_ONLY(dev_bool klaasAssertCheck = false);
bool CStreamingCleanup::MakeSpaceForMap(datResourceMap& map, strIndex index, bool keepMemory)
{
	PF_FUNC(MakeSpaceForMap);

	USE_MEMBUCKET(MEMBUCKET_DEFAULT);
	STRVIS_SET_STREAMABLE_NEEDING_MEMORY(index);

	if (!index.IsInvalid())
	{
		// Reset the flag indicating that an allocation failed.
		// Note that we're not allowed to free up memory for another file on auxiliary threads.
		// streamAssert if somebody tries to.
		streamAssert(sysThreadType::IsUpdateThread());
		m_LastAllocationWasAborted = false;
	}

	if (sysThreadType::IsUpdateThread())
	{
		m_LastAllocationWasFragmented = false;
	}

	size_t virtualSize = 0;
	size_t physicalSize = 0;

	for (int i=0; i<map.VirtualCount; ++i)
	{
		virtualSize += pgRscBuilder::ComputeLeafSize(map.Chunks[i].Size, false);
	}

	for (int i=map.VirtualCount; i<map.VirtualCount+map.PhysicalCount; ++i)
	{
		physicalSize += pgRscBuilder::ComputeLeafSize(map.Chunks[i].Size, true);
	}

#if ONE_STREAMING_HEAP
	physicalSize += virtualSize;
	virtualSize = 0;
#endif // ONE_STREAMING_HEAP

	bool deleteFromList = true;

#if STREAM_STATS
	m_numCleanupCalls++;
	m_numValidationAttempts++;
#endif // STREAM_STATS


	strStreamingAllocator& allocator = strStreamingEngine::GetAllocator();

	while (true)
	{
		bool fragmented = false;

#if __DEV
		OUTPUT_ONLY(size_t physMemoryUsed = allocator.GetPhysicalMemoryUsed();)
#if !ONE_STREAMING_HEAP
		OUTPUT_ONLY(size_t virtMemoryUsed = allocator.GetVirtualMemoryUsed();)
#endif // ONE_STREAMING_HEAP
#endif // __DEV

		bool enoughMemory =	HasSpaceFor(allocator, virtualSize, physicalSize);

		if (enoughMemory)
		{
			MEM_USE_USERDATA(~0U);
			fragmented = !pgRscBuilder::AllocateMap(map);
		}

		if (enoughMemory && !fragmented)
		{
			// We have enough memory, and we are able to allocate the map. Mission accomplished.
			break;
		}

		streamDebugf2("Need to make room for %" SIZETFMT "dKB/%" SIZETFMT "dKB for strIndex %d", virtualSize / 1024, physicalSize / 1024, index.Get());

		STR_TELEMETRY_MESSAGE_WARNING("(streaming/memory)Unable to allocate v=%dK,p=%dK memory - fragmented=%d, for %d (%s)", virtualSize / 1024, physicalSize / 1024, fragmented, index.Get(), index.IsValid() ? strStreamingEngine::GetObjectName(index) : "");

#if STREAM_STATS
		m_numValidationAttempts++;
#endif // STREAM_STATS

#if __ASSERT
		for (int i = 0; i < map.VirtualCount + map.PhysicalCount; ++i)
		{
			streamAssertf(map.Chunks[i].DestAddr == NULL, "Failed to delete all the memory during cleanup");		
		}
#endif 

		deleteFromList = deleteFromList && strStreamingEngine::GetInfo().DeleteNextLeastUsedObject(STRFLAG_LOADSCENE, NULL);
		if (deleteFromList)
		{
			STR_TELEMETRY_MESSAGE_LOG("(streaming/memory/cleanup)Freed up unused objects, trying again");
			continue;
		}

		// Start deleting entities until we free up some memory.
		CleanupResult cleanupResult;

		{
			STRVIS_AUTO_CONTEXT(strStreamingVisualize::MEMORYFREE);
			cleanupResult = FindLowestScoringEntityToDelete(index, STR_DONTDELETE_MASK | STRFLAG_LOADSCENE);
		}

		// If we freed up memory, or if we decided that this object ain't worth it, abort.
		if (cleanupResult != CLEANUP_FAILED)
		{
			if (cleanupResult == CLEANUP_SUCCESS)
			{
				STR_TELEMETRY_MESSAGE_LOG("(streaming/memory/cleanup)Clean-up successful for %d (%s) - trying again", index.Get(), index.IsValid() ? strStreamingEngine::GetObjectName(index) : "");
				// Don't assume that everything worked out - let's just try again and make sure
				// the AllocateMap() call succeeds.	Particularly on 360 with its two heaps,
				// we may still not be able to allocate our map.
				continue;
			}

#if USE_RESOURCE_CACHE
			// If we couldn't free up memory in any other way, we may just need
			// to increase our available heap.
			if (HandleOutOfMemory(GetVirtualSize(map), GetPhysicalSize(map)))
			{
				// If that was an option, let's try one more time to allocate memory.
				if (keepMemory && pgRscBuilder::AllocateMap(map))
				{
					break;
				}
			}
#endif // USE_RESOURCE_CACHE

			// We decided it ain't worth it.
			STR_TELEMETRY_MESSAGE_WARNING("(streaming/memory)Giving up on allocating memory");
			STRVIS_SET_STREAMABLE_NEEDING_MEMORY(strIndex());
			return false;
		}

		streamDebugf2("Unable to make room by deleting entities, now trying packfiles");

		// Let's try packfiles again, unless we don't need RscVirt.
		if (!__PPU || virtualSize)
		{
			if (strPackfileManager::DeleteLRUPackfile())
			{
				streamDebugf3("Deleted packfile - let's try again");
				continue;
			}
		}

		// Let's delete objects - including archetypes, and things that were added this frame.
		strStreamingEngine::GetInfo().ResetInfoIteratorForDeletingNextLeastUsed();
		strStreamingEngine::GetInfo().ClearLastLoaded();
		if (strStreamingEngine::GetInfo().DeleteNextLeastUsedObject(STRFLAG_LOADSCENE, NULL))
		{
			// Got something - let's try again.
			continue;
		}

		streamDebugf2("Now trying vehicles and peds");

		// Try a vehicle or ped then.
		STR_TELEMETRY_MESSAGE_LOG("(streaming/memory/cleanup)Deleting vehicles and peds");
		if (!DeleteVehiclesAndPeds(false))
		{
			STR_TELEMETRY_MESSAGE_LOG("(streaming/memory/cleanup)Deleting vehicles and peds, more severely");
			if (!DeleteVehiclesAndPeds(true))
			{
				STR_TELEMETRY_MESSAGE_WARNING("(streaming/memory/cleanup)Unable to free up any memory");
				streamDebugf2("Unable to free up enough memory (%" SIZETFMT "uk/%" SIZETFMT "uk) for %s", virtualSize / 1024, physicalSize / 1024,
					(index.IsValid()) ? strStreamingEngine::GetInfo().GetObjectName(index) : "[anonymous]");

				// We're fucked, but let's try next frame to free up HD vehicles.
				CVehicle::SetDisableHDVehicles();


#if __DEV
				size_t physMemUsedNow = allocator.GetPhysicalMemoryUsed();
				if (physMemoryUsed != physMemUsedNow)
				{
					streamWarningf("Physical memory used changed from %" SIZETFMT "uKB to %" SIZETFMT "uKB - either another thread changed it, or memory was freed even though we thought it wasn't.",
						physMemoryUsed / 1024, physMemUsedNow / 1024);
				}

#if !ONE_STREAMING_HEAP
				size_t virtMemUsedNow = allocator.GetVirtualMemoryUsed();
				if (virtMemoryUsed != virtMemUsedNow)
				{
					streamWarningf("Virtual memory used changed from %dKB to %dKB - either another thread changed it, or memory was freed even though we thought it wasn't.",
						virtMemoryUsed / 1024, virtMemUsedNow / 1024);
				}
#endif // !ONE_STREAMING_HEAP
#endif // __DEV

#if USE_RESOURCE_CACHE
				// If we couldn't free up memory in any other way, we may just need
				// to increase our available heap.
				if (HandleOutOfMemory(GetVirtualSize(map), GetPhysicalSize(map)))
				{
					// If that was an option, let's try one more time to allocate memory.
					if (keepMemory && pgRscBuilder::AllocateMap(map))
					{
						break;
					}
				}
#endif // USE_RESOURCE_CACHE

#if !__NO_OUTPUT && __DEV
				if (fragmented)
				{
#if ONE_STREAMING_HEAP 
						streamWarningf("Unable to allocate resource due to fragmentation - %dKB free, %" SIZETFMT "uKB needed for %s", int(allocator.GetPhysicalMemoryAvailable() - physMemoryUsed) / 1024, physicalSize / 1024, strStreamingEngine::GetObjectName(index));
#else // ONE_STREAMING_HEAP
						streamWarningf("Unable to allocate resource due to fragmentation - Virtual %dKB available %dKB free %dKB total, Physical %dKB available %dKB free %dKB total, for %s",
							virtualSize / 1024, allocator.GetVirtualMemoryAvailable() / 1024, virtualSize / 1024,
							physicalSize / 1024, allocator.GetPhysicalMemoryAvailable() / 1024, physicalSize / 1024,
							strStreamingEngine::GetObjectName(index));
#endif // ONE_STREAMING_HEAP
				}
#endif // !__NO_OUTPUT && __DEV

				if (sysThreadType::IsUpdateThread())
				{
					m_LastAllocationWasFragmented = fragmented;
				}

#if !__FINAL
				MarkFailedAllocation();
#endif // !__FINAL
				STRVIS_SET_STREAMABLE_NEEDING_MEMORY(strIndex());
				return false;
			}
		}

		// Although we weren't able to free up the entire amount we wanted,
		// it may still be enough to allocate the memory our map needs. Let's
		// try again.
		continue;
	}

	if (!keepMemory)
	{
		MEM_USE_USERDATA(~0U);
		pgRscBuilder::FreeMemory(map);
	}
	STRVIS_SET_STREAMABLE_NEEDING_MEMORY(strIndex());

	return true;
}

#if !__FINAL
void CStreamingCleanup::MarkFailedAllocation()
{
	u32 timeDiff = TIME.GetFrameCount() - m_LastFailedAlloc;

	// Don't spew this more than once every minute or so.
	if (timeDiff > 30 * 60)
	{
		streamWarningf("Failed allocation - current memory usage as follows:");
		CStreamingDebug::DumpModuleMemoryComparedToAverage();

		m_LastFailedAlloc = TIME.GetFrameCount();
	}
}
#endif // !__FINAL

void CStreamingCleanup::RequestFlush(bool loadScene)
{
	m_flushRequested = true;
	m_loadSceneAfterFlush = loadScene;
	m_flushProcessed = false;
}

void CStreamingCleanup::RequestFlushNonMission(bool scriptsOnly)
{
	strStreamingEngine::GetInfo().PurgeRequestList(STR_DONTDELETE_MASK|STRFLAG_FORCE_LOAD|STRFLAG_LOADSCENE);
	strStreamingEngine::GetLoader().Flush();

	strStreamingInfoManager &im = strStreamingEngine::GetInfo();

#if SANITY_CHECK_REQUESTS
	im.SanityCheckRequests();
#endif

	bool removedObjectThisCycle;
	strStreamingModule *scriptModule = &g_StreamedScripts;

	do 
	{
		removedObjectThisCycle = false;
		LoadedInfoList::Iterator it(im.GetLoadedInfoList()->GetFirst());

		while (it.IsValid())
		{
			strIndex index = it.Item()->m_Index;
			it.Next();

			strStreamingInfo& info = im.GetStreamingInfoRef(index);

			strStreamingModule* pModule = im.GetModule(index);

			if (!scriptsOnly || pModule == scriptModule)
			{
				strLocalIndex objIndex = pModule->GetObjectIndex(index);
				int refCount = pModule->GetNumRefs(objIndex);
				int depCount = info.GetDependentCount();

				if (refCount == 0 && depCount == 0)
				{
					if (!(info.GetFlags() & STRFLAG_MISSION_REQUIRED))
					{
						if (im.RemoveObject(index))
						{
							streamDebugf2("Deleting %s", strStreamingEngine::GetObjectName(index));
							removedObjectThisCycle = true;

							// TODO: Don't break.
							break;
						}
						else
						{
							streamDebugf1("Can't delete %s", strStreamingEngine::GetObjectName(index));
						}
					}
					else
					{
						streamDebugf2("Not Deleting %s", strStreamingEngine::GetObjectName(index));
					}
				}
				else
				{
					streamDebugf2("Not deleting %s, ref count=%d, dep count=%d", strStreamingEngine::GetObjectName(index), refCount, depCount);
				}
			}
		}

	} while (removedObjectThisCycle);

	streamDisplayf("GC Flush complete");
#if LIVE_STREAMING
	strStreamingEngine::GetLive().UpdateAllDirtyFiles();
#endif // LIVE_STREAMING
}

void CStreamingCleanup::Flush()
{
	m_flushProcessed = true;

#if NAVMESH_EXPORT
	const bool bCalledFromNavmeshExporter = CNavMeshDataExporter::IsExportingCollision();
#else
	const bool bCalledFromNavmeshExporter = false;
#endif

#if __BANK
	if(!bCalledFromNavmeshExporter)
	{
		CDebugTextureViewerInterface::Close(true);
		CAnimPlacementEditor::DeactivateBank();
		if (CAnimViewer::m_pBank->IsActive())
		{
			CAnimViewer::m_pBank->ToggleActive();
		}
		CAnimClipEditor::ms_previewClipOnFocusEntity = false;
		CAnimClipEditor::StartPreviewClip();
	}

	clothManager::SetSceneIsFlushing(true); //for bugstar 2253343, will be removed in the future!
#endif // __BANK

	CStreaming::LoadAllRequestedObjects();
	CTexLod::UnbindAllBoundTxds();
	CZonedAssetManager::Reset();

	gRenderThreadInterface.Flush();

	if (m_FlushMapStore)
	{
		INSTANCE_STORE.SafeRemoveAll(false);
	}

	CPlayerInfo* pPlayerInfo = CGameWorld::GetMainPlayerInfo();
	Vector3 playerPos = pPlayerInfo ? pPlayerInfo->GetPos() : CPlayerInfo::ms_cachedMainPlayerPos;
	CGameWorld::ClearExcitingStuffFromArea(playerPos, 4000.0f, true, true, false, false, NULL, true, true);

	// Vehicles
	if (m_FlushVehicles)
	{
		CTrain::RemoveAllTrains();
		CVehiclePopulation::RemoveAllVehsHard();
	}

	CDispatchManager::GetInstance().Flush();

	if (m_FlushPeds)
	{
		// Peds
		{
			CPed::Pool*	pool	= CPed::GetPool();
			s32		i		= pool->GetSize();
			while(i--)
			{
				CPed* pPed = pool->GetSlot(i);
				if(pPed && !pPed->IsPlayer())
				{
					CPedFactory::GetFactory()->DestroyPed(pPed);
				}	
			}
		}
	}

	// Make sure we don't try create any scheduled peds after the flush
	CPedPopulation::FlushPedQueues();

	if (m_FlushObjects)
	{
		// Objects
		{
			CObject::Pool *pool = CObject::GetPool();
			CObject* pObject;
			s32 i = (s32) pool->GetSize();

			while(i--)
			{
				pObject = pool->GetSlot(i);
				if (pObject)
				{
					if (pObject->GetOwnedBy() != ENTITY_OWNEDBY_SCRIPT)
					{
						CObjectPopulation::ConvertToDummyObject(pObject, true);
					}
#if !__FINAL
					else
					{
						streamDisplayf("CStreamingCleanup::Flush - ConvertToDummyObject() not called for script object %s", pObject->GetModelName());
					}
#endif	//	!__FINAL
				}
			}
		}

		CPickupManager::RemoveAllPickups(true, true);
	}

	if (m_FlushCollisions)
	{
		// Flush all collisions.
		g_StaticBoundsStore.RemoveAll();
	}

	//Vector3 playerPos = pPlayerInfo->GetPos();

	if (m_FlushPeds)
	{
		CPed * pPlayerPed = FindPlayerPed();
		streamAssert(pPlayerPed);
		if(pPlayerPed)
		{
			CPedFactory::GetFactory()->DestroyPlayerPed( pPlayerPed );

			gPopStreaming.Shutdown(SHUTDOWN_SESSION);
			CPedVariationPack::FlushSlods();
		}
	}

	if (m_FlushClouds)
	{
		CLOUDHATMGR->Flush(true);
	}

	CEntityIterator entityIterator(IterateAllTypes);
	CEntity* pEntity = entityIterator.GetNext();
	while (pEntity) {
		pEntity->DeleteDrawable();
		pEntity = entityIterator.GetNext();
	}

	if (m_FlushClips)
	{
#if __BANK
		if(!bCalledFromNavmeshExporter)
		{
			if(CAnimClipEditor::m_pClip)
			{
				if (CAnimClipEditor::m_pClip)
				{
					CAnimClipEditor::m_pClip->Release();
					CAnimClipEditor::m_pClip = NULL;
				}
			}
		}
#endif // __BANK

		fwClipSetManager::Shutdown(SHUTDOWN_WITH_MAP_LOADED);

		//reload the move networks
		fwAnimDirector::ShutdownStatic(SHUTDOWN_WITH_MAP_LOADED);
	}

	DRAWLISTMGR->ClothCleanup();
	DRAWLISTMGR->CharClothCleanup();

	if (m_FlushCloth)
	{
		g_ClothStore.ShutdownLevel();
	}

	CPhysics::GetRopeManager()->Deactivate();

	g_fwMetaDataStore.RemoveAll();

	strStreamingEngine::GetInfo().ResetInfoIteratorForDeletingNextLeastUsed();
	while (strStreamingEngine::GetInfo().DeleteNextLeastUsedObject(STR_DONTDELETE_MASK, NULL, false)) {}

	strStreamingEngine::GetInfo().PurgeRequestList(STR_DONTDELETE_MASK|STRFLAG_FORCE_LOAD|STRFLAG_LOADSCENE);

#if __BANK
	strPackfileManager::EvictUnusedImages();
	clothManager::SetSceneIsFlushing(false); //for bugstar 2253343, will be removed in the future!
#endif // __BANK

	strStreamingEngine::GetLoader().Flush();

	strStreamingEngine::GetInfo().FlushLoadedList();
#if LIVE_STREAMING
	strStreamingEngine::GetLive().UpdateAllDirtyFiles();
#endif // LIVE_STREAMING

	if (m_FlushPeds)
	{
		gPopStreaming.Init(INIT_SESSION);
		CPedVariationPack::LoadSlods();
	}
	
	if (m_FlushClips)
	{
		fwAnimDirector::InitStatic(INIT_AFTER_MAP_LOADED);
		fwClipSetManager::Init(INIT_AFTER_MAP_LOADED);
#if __BANK
		if(!bCalledFromNavmeshExporter)
		{
			if(CAnimClipEditor::m_pClip)
			{
				CAnimClipEditor::m_pClip = CAnimClipEditor::m_clipEditAnimSelector.GetSelectedClip();
				if (CAnimClipEditor::m_pClip)
				{
					CAnimClipEditor::m_pClip->AddRef();
				}
			}
		}
#endif // __BANK
	}

	if (m_loadSceneAfterFlush)
		g_SceneStreamerMgr.LoadScene(playerPos);

	if (m_FlushPeds)
	{
		if(!bCalledFromNavmeshExporter)
		{
			const CModelInfoConfig& cfgModel = CGameConfig::Get().GetConfigModelInfo();
			CGameWorld::CreatePlayer(playerPos, atStringHash(cfgModel.m_defaultPlayerName), 200.0f);
		}
	}

	if (m_FlushClouds)
	{
		CLOUDHATMGR->Flush(false);
	}
}

#if STREAMING_DETAILED_STATS
void CStreamingCleanup::MarkDrawHandlerDeleted(CEntity *entity, float score)
{
	// Overrun check.
	if (m_LastDeletedHandlers[m_LastDeletedHandlerIndex].IsFull())
	{
		streamAssertf(false, "Array overflow - increase MAX_DELETED_COUNT. Don't open a bug, just mail the Krehan.");
		return;
	}

	DeletedDrawableHandler handlerInfo;
	handlerInfo.m_BucketScore = score;
	m_LastDeletedHandlers[m_LastDeletedHandlerIndex].Insert(entity, handlerInfo);
}

void CStreamingCleanup::MarkDrawHandlerCreated(CEntity *entity, float score)
{
	int mapIndex = m_LastDeletedHandlerIndex;
	int frameCount = 0;

	for (int x=0; x<MAX_DELETED_CHURN_FRAMES; x++)
	{
		--mapIndex;
		if (mapIndex < 0)
		{
			mapIndex = MAX_DELETED_CHURN_FRAMES - 1;
		}

		DeletedDrawableHandler *handlerInfo = m_LastDeletedHandlers[mapIndex].Access(entity);

		if (handlerInfo)
		{
			m_ChurnCounts[frameCount]++;

			// CHURN! This had just been removed!
			streamDisplayf("CHURN (frame count: %d). Entity %s (%p) was deleted with score %f, recreated with score %f.",
				frameCount, entity->GetModelName(), entity, handlerInfo->m_BucketScore, score);
			break;
		}

		frameCount++;
	}
}

void CStreamingCleanup::DisplayChurnStats()
{
	char churnString[192];
	churnString[0] = 0;

	for (int x=0; x<MAX_DELETED_CHURN_FRAMES; x++)
	{
		char substring[128];

		formatf(substring, "%d: %d   ", x, m_ChurnCounts[x]);
		safecat(churnString, substring);
	}

	grcDebugDraw::AddDebugOutputEx(false, "Entity draw handler churn: %s", churnString);
}

#endif // STREAMING_DETAILED_STATS

#if __BANK
void CStreamingCleanup::DisplayChurnPoint()
{
	grcDebugDraw::AddDebugOutputEx(false, "Most recent churn point: %f (age: %d)", m_LastChurnPoint, TIME.GetFrameCount() - m_LastChurnTimestamp);
}

void CStreamingCleanup::AddWidgets(bkGroup &group)
{
	bkGroup *flushGroup = group.AddGroup("Flush Control");

	flushGroup->AddToggle("Flush Map Store", &m_FlushMapStore, datCallback(MFA(CStreamingCleanup::UpdateNonClipsFlushControls), this));
	flushGroup->AddToggle("Flush Vehicles", &m_FlushVehicles, datCallback(MFA(CStreamingCleanup::UpdateNonClipsFlushControls), this));
	//flushGroup->AddToggle("Flush Peds", &m_FlushPeds, datCallback(MFA(CStreamingCleanup::UpdateNonClipsFlushControls), this));
	flushGroup->AddToggle("Flush Collisions", &m_FlushCollisions, datCallback(MFA(CStreamingCleanup::UpdateNonClipsFlushControls), this));
	flushGroup->AddToggle("Flush Clips", &m_FlushClips, datCallback(MFA(CStreamingCleanup::UpdateClipsFlushControls), this));
	flushGroup->AddToggle("Flush Objects", &m_FlushObjects, datCallback(MFA(CStreamingCleanup::UpdateNonClipsFlushControls), this));
	flushGroup->AddToggle("Flush Cloth", &m_FlushCloth, datCallback(MFA(CStreamingCleanup::UpdateNonClipsFlushControls), this));
	flushGroup->AddToggle("Flush Clouds", &m_FlushClouds, datCallback(MFA(CStreamingCleanup::UpdateNonClipsFlushControls), this));
	flushGroup->AddButton("Flush Now", datCallback(MFA1(CStreamingCleanup::RequestFlush), this, NULL));
	flushGroup->AddButton("Flush Non-Mission Required Scripts Only", datCallback(MFA1(CStreamingCleanup::RequestFlushNonMission), this, (CallbackData) true));
	flushGroup->AddButton("Flush - Garbage Collect Everything Non-Mission Required", datCallback(MFA1(CStreamingCleanup::RequestFlushNonMission), this, NULL));
}

void CStreamingCleanup::UpdateClipsFlushControls()
{
	// Flush animations requires flushing all peds.
	if (m_FlushClips)
	{
		m_FlushVehicles = true;
		m_FlushObjects = true;
		m_FlushCloth = true;
		//m_FlushPeds = true;
	}
}

void CStreamingCleanup::UpdateNonClipsFlushControls()
{
	if (!m_FlushVehicles || !m_FlushCloth || !m_FlushObjects)
	{
		m_FlushClips = false;
	}
}

#endif // __BANK
