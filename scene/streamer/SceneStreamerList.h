/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/streamer/SceneStreamerList.h
// PURPOSE : a small class to represent an entity list for scene streaming
// AUTHOR :  Ian Kiigan
// CREATED : 19/07/11
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _SCENE_STREAMER_SCENESTREAMERLIST_H
#define _SCENE_STREAMER_SCENESTREAMERLIST_H

#include "game_config.h"

#include "atl/array.h"
#include "fwscene/scan/VisibilityFlags.h"

#include "renderer/PostScan.h"
#include "scene/ContinuityMgr.h"
#include "scene/lod/LodMgr.h"
#include "scene/streamer/SceneStreamerBase.h"
#include "scene/streamer/StreamVolume.h"
#include "spatialdata/sphere.h"
#include "streaming/streamingvisualize.h"
#include "vector/vector3.h"

struct BucketEntry;
class CEntity;
class CGtaPvsEntry;


// a list of scene streaming entities
class CSceneStreamerList
{
public:

	enum StreamModeFlags
	{
		// If set, we're moving fast, so we should prefer LODs over HDs.
		DRIVING_LOD	= (1 << 0),

		// Skip the priority check
		SKIP_PRIORITY = (1 << 1),

		// If set, we're moving fast, so we should reduce the priority for non-essential interiors.
		DRIVING_INTERIOR	= (1 << 2),
	};

	enum
	{
		ALLOC_STEP		= 100,
		MIN_SIZE		= 100,
		MAX_OBJ_INSTANCES = MAX_ACTIVE_ENTITIES,
	};
	enum eListType
	{
		TYPE_NONE,
		TYPE_PVS,
		TYPE_VOLUME,
		TYPE_IMAPGROUP
	};

	CSceneStreamerList() : m_listType(TYPE_NONE) { m_entityList.Reserve(MIN_SIZE); }

	u32 GetNumEntries() const { return m_entityList.GetCount(); }
	CEntity* GetEntryAt(u32 index) { return m_entityList[index]; }
	void SubmitEntity(CEntity* pEntity);
	eListType GetType() const { return m_listType; }
	void SetType(eListType listType) { m_listType = listType; }

private:
	atArray<CEntity*> m_entityList;
	eListType m_listType;
};

// ---- New streaming system -----


/** PURPOSE: A bucket set has a bunch of virtual buckets, each one containing an ordered
 *  list of entity pointers.
 *
 *  The actual implementation is actually one large array of entities and their score,
 *  the score being a float with the integer indicating the bucket number and the decimal
 *  being the importance within the bucket. As such, walking through this array simulates
 *  going from the first bucket to the last, from most important to least important element
 *  in each.
 */
struct CStreamingBucketSet
{
public:
	CStreamingBucketSet();

	/** PURPOSE: Initialize the bucket entry array with a given capacity. */
	void InitArray(int size)		{ m_EntityList.Reserve(size); }

	/** PURPOSE: Return total number of elements in the array */
	int GetEntryCount() const		{ return m_EntityList.GetCount(); }

	/** PURPOSE: Return the pointer to the first element in the array. */
	BucketEntry *GetEntries()		{ return m_EntityList.GetElements(); }

	/** PURPOSE: Return the pointer to the first element in the array. */
	const BucketEntry *GetEntries() const	{ return m_EntityList.GetElements(); }

	/** PURPOSE: Accessor for the element array. */
	BucketEntry::BucketEntryArray &GetBucketEntries()	{ return m_EntityList; }

	/** PURPOSE: Add an entity to the list. This will simply add the entity
	 *  to the end of the list, you will need to call SortBuckets() after everything
	 *  has been added to sort the list.
	 *
	 *  PARAMS:
	 *   score - Score of the entity. The integer indicates the bucket, the decimal is
	 *           the importance within the bucket.
	 */
	__forceinline void AddEntry(float score, CEntity *entity, u32 entityID);

	/** PURPOSE: Remove all entries from the list. */
	void Reset();

	/** PURPOSE: Kicks off an asynchronous process that add entities from the linked list to the bucket set and then sorts it */
	void AddActiveEntitiesToBucketAndSort(const fwLink<ActiveEntity>* pStore = NULL, u32 storeSize = 0, const fwLink<ActiveEntity>* pFirst = NULL, const fwLink<ActiveEntity>* pLast = NULL);
	
	/** PURPOSE: Sort all elements in the array by their score. */
	void SortBuckets(bool forcePPU = false);

	/** PURPOSE: Stall the thread until the SPU (or worker thread) is done sorting the
	 *  entries. Ideally, the task is done by the time this function is called.
	 */
	void WaitForSorting();

#if __BANK
	void ShowBucketUsage(bool showScoreMinMax);
	void ShowBucketContents(int bucketOnly = -1);
	void ShowRemovalStats();
	void TrackRemoval()										{ m_Removed++; }
	void UpdateStats();

	void TrackDeletion(int bucket)							{ m_DeletionThisFrame[bucket]++; m_TotalDeletion[bucket]++; }
	void TrackRequest(int bucket)							{ m_RequestsThisFrame[bucket]++; }
#endif // __BANK

	static __forceinline float IdentifyBucket(CGtaPvsEntry *entry, CEntity *entity, const fwVisibilityFlags& visFlags, const bool isVisibleInGBufOrMirror, s32 interiorProxyId, s32 roomId, Vector3::Vector3Param vCamPos, Vector3::Vector3Param vCamDir, u32 streamMode);

#if !__FINAL
	static const char *GetStreamingBucketName(StreamBucket bucketId);
#endif // !__FINAL

#if __BANK
	static bool *GetUseSPUSortPtr()			{ return &sm_UseSPUSort; }
	static bool *GetUseQSortPtr()			{ return &sm_UseQSort; }
#endif // __BANK

#if !__FINAL
	void DumpAllEntitiesOnce();
#endif // !__FINAL

private:
	static bool IsEntityFarAway(float distance, const CEntity *entity, Vector3::Vector3Param vCamPos, Vector3::Vector3Param vCamDir);
	static bool IsNearbyInteriorShell(CGtaPvsEntry *entry, CEntity *entity);

	static int SortFuncQSort(const BucketEntry *a, const BucketEntry *b);

	sysDependency m_SortDependency;

	// List of all entities, sorted by their score.
	BucketEntry::BucketEntryArray m_EntityList;

	u32 m_SortDependencyRunning;

#if !__FINAL
	u32 m_DumpAllEntities;
#endif // !__FINAL

#if __BANK
	int m_Removed;
	float m_AverageRemoved;
	atRangeArray<u32, STREAMBUCKET_COUNT> m_BucketCounts;
	atRangeArray<int, STREAMBUCKET_COUNT> m_DeletionThisFrame;
	atRangeArray<int, STREAMBUCKET_COUNT> m_TotalDeletion;
	atRangeArray<int, STREAMBUCKET_COUNT> m_RequestsThisFrame;
#endif // __BANK

	static bool sm_UseSPUSort;
	static bool sm_UseQSort;
};

/** PURPOSE: This list manages all entities that are currently active, i.e. that
 *  have a draw handler. There are two parts to it: A CStreamingBucketSet, and
 *  a linked list of active entities.
 */
class CActiveEntityList
{
public:
	void Init();
	void Shutdown();

	void Update();

	/** PURPOSE: Add an entity to the linked list of active entities. Note that this
	 *  function will NOT add the entity to the bucket set.
	 */
	fwLink<ActiveEntity>* AddEntity(fwEntity* pEntity);

	/** PURPOSE: Remove an entity from the linked list of active entities. Note that this
	 *  function will NOT remove the entity from the bucket set.
	 */
	void RemoveEntity(fwLink<ActiveEntity>* pLink);

	/** PURPOSE: Accessor for the linked list of active entities. */
	const fwLinkList<ActiveEntity>& GetDrawableLinkList() const { return m_ActiveEntities; }

	/** PURPOSE: Accessor for the bucket set of active entities. */
	CStreamingBucketSet &GetBucketSet() { return m_ActiveBucketSet; }

private:
	fwLinkList<ActiveEntity> m_ActiveEntities;
	CStreamingBucketSet m_ActiveBucketSet;	
};

/** PURPOSE: This list manages all entities that are currently needed, i.e. they do not
 *  have a draw handler, but the PVS decided that we should stream these entities in.
 */
class CNeededEntityList
{
public:
	/** PURPOSE: Accessor for the bucket set of active entities. */
	CStreamingBucketSet &GetBucketSet()							{ return m_NeededBucketSet; }

private:
	CStreamingBucketSet m_NeededBucketSet;	
};


/** PURPOSE: This classs manages the two main classes for streaming requests and clean-up, which
 *  would be the active list (CActiveEntityList) and the needed list (CNeededEntityList).
 */
class CStreamingLists
{
public:
	CStreamingLists();

	void Init();
	void Shutdown();

	/** PURPOSE: Add an entity to the linked list of active entities. Note that this
	 *  function will NOT add the entity to the bucket set.
	 */
	fwLink<ActiveEntity>* AddActive(fwEntity *entity)		{ return m_ActiveEntityList.AddEntity(entity); }

	bool AddNeeded(CGtaPvsEntry *pvsEntry, CEntity *pEntity, const fwVisibilityFlags& visFlags, const bool isVisibleInGBufOrMirror, u32 streamMode, bool skipVisFlags);

	/** PURPOSE: Remove an entity from the the bucket set it's in, either the active or needed one. */
	//void RemoveEntity(CEntity &entity);

	/** PURPOSE: Accessor for the linked list of active entities. */
	const fwLinkList<ActiveEntity>& GetActiveList() const	{ return m_ActiveEntityList.GetDrawableLinkList(); }

	CActiveEntityList& GetActiveEntityList()			{ return m_ActiveEntityList; }
	CNeededEntityList& GetNeededEntityList()			{ return m_NeededEntityList; }

	Vec3V_Out GetCameraPositionForFade() const			{ return RCC_VEC3V(m_CameraPositionForFade); }
	Vec3V_Out GetCameraDirection() const				{ return RCC_VEC3V(m_CameraDirection); }

	void UpdateInteriorAndCamera();

	/** PURPOSE: This function takes the result of the PVS and moves all entities either in the active
	 *  list if they have a draw handler, or in the needed list. Existing entities with draw handlers that
	 *  were not in the PVS will also be added to the active list, although in the least favorable
	 *  bucket (active outside PVS).
	 */
	__forceinline void ProcessPvsEntry(CGtaPvsEntry* pvsEnty, CEntity* entity, const fwVisibilityFlags& visFlags, const bool isVisibleInGBufOrMirror, CEntityDrawHandler* drawHandler, const u32 lastVisibleFrame, u32 streamMode);

	void CreateDrawablesForEntities();

	void AddFromMapGroupSwap();
	void AddFromStreamingVolume();
	void AddFromInteriorRoomZero(CInteriorInst* pIntInst);
	void AddFromRequiredInteriors();

	void AddAdditionalEntity(CEntity *pEntity, bool skipVisCheck, u32 streamMode);

	void PreGenerateLists();
	void PostGenerateLists();

#if STREAMING_VISUALIZE
/*	void UpdateVisibilityToStreamingVisualize();
	void AddActiveForStreamingVisualize(const fwEntity *entity);
	void RemoveActiveForStreamingVisualize(const fwEntity *entity);*/
#endif // STREAMING_VISUALIZE

	void WaitForSort();

	/** PURPOSE: Returns a different value each time the list has been recreated. */
	u32 GetTimestamp() const							{ return m_Timestamp; }

#if __BANK
	void AddWidgets(bkGroup &group);

	void ShowBucketUsage();
	void ShowBucketContents();
	void ShowRemovalStats();

	void SetSceneStreamingEnabled(bool bEnabled);
	void TrackCutoff(float cutoff)						{ m_Cutoff = cutoff; }

#endif // __BANK

	bool *GetDriveModePtr()								{ return &m_IsDriveModeEnabled; }
	bool *GetInteriorDriveModePtr()						{ return &m_IsInteriorDriveModeEnabled; }
	float *GetReadAheadFactorPtr()						{ return &m_ReadAheadFactor; }

	void MarkCutoffEvent(float cutoff)					{ m_MasterCutoff = Max(m_MasterCutoff, cutoff); }
	float GetMasterCutoff() const						{ return m_MasterCutoff; }
	void SetMasterCutoff(float cutoff)					{ m_MasterCutoff = cutoff; }

	u32 GetStreamMode() const							{ return m_StreamMode; }
	void SetStreamMode(u32 streamMode)					{ m_StreamMode = streamMode; }

	static bool IsRequired(const CEntity *entity);

private:

#if __BANK
	void UpdateVisibilitySets();
#endif // __BANK


	CActiveEntityList m_ActiveEntityList;
	CNeededEntityList m_NeededEntityList;
	atFixedArray<CEntity*, 512> m_EntitiesToCreateDrawables;

	u32 m_Timestamp;
	float m_MasterCutoff;
	float m_ReadAheadFactor;

	u32 m_StreamMode;					// Flags from StreamModeFlags
	s32 m_CurrentInteriorProxyId;
	s32 m_CurrentRoomId;
	Vector3 m_CameraPositionForFade;
	Vector3 m_CameraDirection;

	// Master switch for drive mode - if false, drive mode will never kick in.
	bool m_IsDriveModeEnabled;
	bool m_IsInteriorDriveModeEnabled;

#if __BANK
	float m_Cutoff;
	u32 m_VisibilitySets;
	int m_DisplayOnlyBucket;
	bool m_ShowBucketUsage;
	bool m_ShowActiveBucketContents;
	bool m_ShowNeededBucketContents;
	bool m_ShowRemovalStats;
	bool m_SPUSortForRequests;
	bool m_AddNearbyInteriors;
	bool m_AddStreamVolumes;
	bool m_AddRenderPhasePvs;
	bool m_ShowScoreMinMax;
#endif // __BANK

#if __ASSERT
	bool m_ListLocked;
#endif // __ASSERT
};

__forceinline void CStreamingLists::ProcessPvsEntry(CGtaPvsEntry* pvsEntry, CEntity* entity, const fwVisibilityFlags& visFlags, const bool isVisibleInGBufOrMirror, CEntityDrawHandler* drawHandler, const u32 lastVisibleFrame, u32 streamMode)
{
	Assert((pvsEntry != NULL) && (entity != NULL));

#if __BANK
	// Don't add anything that isn't in any of the sets enabled via RAG.
	if (!(visFlags & m_VisibilitySets))
	{
		return;
	}
#endif // __BANK

	if(drawHandler)
	{
		if(drawHandler->IsInActiveList())
		{
			Assert(IsRequired(entity));
			Assert(!entity->GetIsTypePed() && !entity->GetIsTypeVehicle());

			// We have a visible entity with a draw handler that is in the active entity list. 
			// Mark the draw handler as last seen on this frame, then score the entity and 
			// add it to the bucket set for sorting & use later.
			Assert(drawHandler->GetActiveListLink() && dynamic_cast<fwLink<ActiveEntity>*>(drawHandler->GetActiveListLink()));
			fwLink<ActiveEntity>* pLink = static_cast<fwLink<ActiveEntity>*>(drawHandler->GetActiveListLink());
			pLink->item.MarkVisible(lastVisibleFrame);

			float bucketScore = CStreamingBucketSet::IdentifyBucket(pvsEntry, entity, visFlags, isVisibleInGBufOrMirror, m_CurrentInteriorProxyId, m_CurrentRoomId, m_CameraPositionForFade, m_CameraDirection, streamMode | CSceneStreamerList::SKIP_PRIORITY);
			pLink->item.SetScore(bucketScore);
		}
	}
	else if(!entity->GetIsTypePed() && !entity->GetIsTypeVehicle())
	{
		// No draw handler - Check to see if the model info has loaded. If so, create the drawable 
		// and draw handler now. Successful creation of the drawable & drawHandler will automatically 
		// add it to the active entity list, if applicable.
		CBaseModelInfo* pModelInfo = entity->GetBaseModelInfo();
		if (pModelInfo && pModelInfo->GetHasLoaded())
		{
			if(IsRequired(entity))
			{
				// Push onto array to create drawable later, since this may add entities (e.g., attached lights), 
				// and the scene graph is currently locked. The array has a fixed, limited size. If its full, 
				// then we just don't push onto the list, and later we'll see its full and simply check the 
				// whole PVS for drawables to create.
				if(Likely(!m_EntitiesToCreateDrawables.IsFull()))
				{
					m_EntitiesToCreateDrawables.Append() = entity;
				}

				// So save everyone time and memory, let's not even consider models that are not deletable. Cause we won't be able to delete them.
				strLocalIndex objIndex = strLocalIndex(entity->GetModelIndex());
				strStreamingModule* streamingModule = fwArchetypeManager::GetStreamingModule();
				strIndex archIndex = streamingModule->GetStreamingIndex(objIndex);

				if (!(strStreamingEngine::GetInfo().GetStreamingInfoRef(archIndex).GetFlags() & STR_DONTDELETEARCHETYPE_MASK))
				{
					// Even though we're going to create the drawable later, we'll assume that will succeed 
					// and simply score it now, because we have the pvsEntry handy.
					float bucketScore = CStreamingBucketSet::IdentifyBucket(pvsEntry, entity, visFlags, isVisibleInGBufOrMirror, m_CurrentInteriorProxyId, m_CurrentRoomId, m_CameraPositionForFade, m_CameraDirection, streamMode | CSceneStreamerList::SKIP_PRIORITY);
					m_ActiveEntityList.GetBucketSet().AddEntry(bucketScore, entity, BucketEntry::CalculateEntityID(entity));
#if STREAMING_DETAILED_STATS
					CStreaming::GetCleanup().MarkDrawHandlerCreated(entity, bucketScore);
#endif // STREAMING_DETAILED_STATS
				}
			}
		}
		else
		{
			// This entity is visible, but has no drawable/draw handler, and its model info isn't loaded, 
			// so we can't create them either. Add it to the scene streamer's needed list, so its model 
			// info can be requested later.
			this->AddNeeded(pvsEntry, entity, visFlags, isVisibleInGBufOrMirror, streamMode, false);
		}
	}
}

__forceinline void CStreamingBucketSet::AddEntry(float score, CEntity *entity, u32 entityID)
{
	if (m_EntityList.GetCount() < m_EntityList.GetCapacity())
	{
#if __BANK
		// Mark this as volatile to force the compiler to do the store of the float-to-int
		// conversion here, but the load later down the road. This will prevent thousands
		// of LHS every frame.
		volatile int bucket = (int) score;
#endif // __BANK

		BucketEntry &entry = m_EntityList.Append();
		entry.SetEntity(entity, entityID);
		entry.m_Score = score;

		PrefetchDC((&entry) + 1);

#if __BANK
		m_BucketCounts[bucket]++;
#endif // __BANK
	}
	else
	{
		// List is full. Is that because of a streaming volume? If so, it's kind of to be expected.
#if __ASSERT
		//CEntity* entity = entityIDHelpers::GetValidatedEntity(entityID);
		//Assertf(CStreamVolumeMgr::IsAnyVolumeActive(), "StreamingBucketSet - cannot add %s as list is full. List dump in the TTY.", entity ? entity->GetModelName() : "NULL");
#endif

#if !__FINAL
		DumpAllEntitiesOnce();
#endif // !__FINAL
	}
}

//												HD     LOD   SLOD1  SLOD2  SLOD3  ORPHAN SLOD4
const float LodAdjust[LODTYPES_DEPTH_TOTAL] = { 0.0f, 0.08f, 0.16f, 0.24f, 0.32f, 0.02f, 0.4f };

__forceinline float CStreamingBucketSet::IdentifyBucket(CGtaPvsEntry *entry, CEntity *entity, const fwVisibilityFlags& visFlags, const bool isVisibleInGBufOrMirror, s32 interiorProxyId, s32 roomId, Vector3::Vector3Param vCamPos, Vector3::Vector3Param vCamDir, u32 streamMode)
{
	FastAssert(!entity->GetIsTypePed() && !entity->GetIsTypeVehicle());

	// Use very fast intrinsic (but not 100% accurate, some loss of precision) 
	// to do 1/dist calc, so we avoid the non-pipelined fdiv penalty.
	float distFactor = Min(FPInvertFast(entry->GetDist() + 0.000001f), 0.95f);

	// If we're interior...
	if (!isVisibleInGBufOrMirror && interiorProxyId != -1)
	{
		fwInteriorLocation loc = entity->GetInteriorLocation();

		if (loc.GetInteriorProxyIndex() != interiorProxyId)
		{
			return (float) STREAMBUCKET_INVISIBLE_EXTERIOR + distFactor;
		}
		else
		{
			if (loc.IsAttachedToRoom() && loc.GetRoomIndex() != roomId)
			{
				return (float) STREAMBUCKET_INVISIBLE_OTHERROOMS + distFactor;
			}
		}
	}

	if (visFlags.GetOptionFlags() & VIS_FLAG_OPTION_INTERIOR)
	{
		if (IsNearbyInteriorShell(entry, entity))
		{
			return STREAMBUCKET_VISIBLE + distFactor;
		}
		else
		{
			if (isVisibleInGBufOrMirror)
			{
				if (streamMode & CSceneStreamerList::DRIVING_INTERIOR)
				{
					fwInteriorLocation loc = entity->GetInteriorLocation();
					if (loc.GetInteriorProxyIndex() != interiorProxyId)
					{
						return (float) STREAMBUCKET_VISIBLE_FAR + distFactor;
					}
					else if (loc.IsAttachedToRoom() && loc.GetRoomIndex() != roomId)
					{
						return (float) STREAMBUCKET_VISIBLE_FAR + distFactor;
					}
				}
			}
		}
	}

	{
		// If it's invisible, it can't be considered high priority.
		if (!isVisibleInGBufOrMirror)
		{
			//////////////////////////////////////////////////////////////////////////
			// hacky special case when driving fast and looking backwards - priority boost offscreen models
			if ( g_ContinuityMgr.ModerateSpeedDriving() && (g_ContinuityMgr.IsLookingBackwards() || g_ContinuityMgr.IsLookingAwayFromDirectionOfMovement()) )
			{
				const fwLodData& lodData = entity->GetLodData();
				if (!lodData.IsContainerLod() && g_LodMgr.WithinVisibleRange(entity))
				{
					if (lodData.IsLoddedHd())
					{
						return STREAMBUCKET_VISIBLE_FAR + distFactor;
					}
					else if (lodData.IsLod() || lodData.IsSlod1())
					{
						return STREAMBUCKET_VISIBLE + distFactor;
					}
				}
			}
			//////////////////////////////////////////////////////////////////////////

			return STREAMBUCKET_INVISIBLE_FAR + distFactor;
		}

		if (streamMode & CSceneStreamerList::DRIVING_LOD)
		{
			distFactor += LodAdjust[entity->GetLodData().GetLodType()];
		}

		if ((streamMode & CSceneStreamerList::SKIP_PRIORITY) || IsEntityFarAway(entry->GetDist(), entity, vCamPos, vCamDir))
		{
			return (float) STREAMBUCKET_VISIBLE_FAR + distFactor;
		}

		return (float) STREAMBUCKET_VISIBLE + distFactor;
	}
}

#endif // _SCENE_STREAMER_SCENESTREAMERLIST_H