/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/streamer/SceneStreamerList.cpp
// PURPOSE : a small class to represent an entity list for scene streaming
// AUTHOR :  Ian Kiigan
// CREATED : 19/07/11
//
/////////////////////////////////////////////////////////////////////////////////

#include "scene/streamer/SceneStreamerList.h"

#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/viewports/ViewportManager.h"
#include "fwscene/lod/LodTypes.h"
#include "fwscene/scan/VisibilityFlags.h"
#include "fwscene/search/Search.h"
#include "fwscene/stores/imapgroup.h"
#include "fwscene/stores/mapdatastore.h"
#include "renderer/PostScan.h"
#include "scene/ContinuityMgr.h"
#include "scene/Entity.h"
#include "scene/EntityBatch.h"
#include "scene/streamer/SceneStreamerMgr.h"
#include "scene/streamer/StreamVolume.h"
#include "scene/world/GameScan.h"
#include "scene/world/GameWorld.h"
#include "peds/ped.h"
#include "camera/viewports/ViewportManager.h"

#if __BANK
#include "fwdebug/vectormap.h"
#include "scene/debug/SceneStreamerDebug.h"
#include "vector/colors.h"
#endif	//__BANK

SCENE_OPTIMISATIONS();

#if !__FINAL
static const char *s_StreamingBucketNames[] = {
	"Active Outside PVS",
	"Invisible Far",
	"Invisible Exterior",
	"Vehs/Peds Cutscene",
	"Invisible Other Rooms",
	"Invisible",
	"Visible Far",
	"Vehicles And Peds",
	"Visible",
	"Active Vehicle/Peds",
};

CompileTimeAssert(NELEM(s_StreamingBucketNames) == STREAMBUCKET_COUNT);
#endif // !__FINAL

#if __STATS
EXT_PF_TIMER(FindLowestScoring);
EXT_PF_TIMER(SortBuckets);
EXT_PF_TIMER(WaitForSPUSort);
EXT_PF_TIMER(IssueRequests);
EXT_PF_VALUE_INT(ActiveEntities);
EXT_PF_VALUE_INT(NeededEntities);
#endif // __STATS

extern	bool g_show_streaming_on_vmap;

static sysCriticalSectionToken	s_CriticalSection;		

#if STREAMING_VISUALIZE
// List of entities that were added to the active list this frame (which presumably means they're visible now)
static atArray<const fwEntity *> s_NewEntitiesForSV;

// List of entities that are no longer visible this frame
static atArray<const fwEntity *> s_NowInvisibleForSV;

// List of entities that have been removed this frame
static atArray<const fwEntity *> s_RemovedEntitiesForSV;

// All entities currently in the active list, and those that have been removed this frame
static atMap<const fwEntity *, bool> s_ActiveEntitiesForSV;
#endif // STREAMING_VISUALIZE

#if __PPU
DECLARE_FRAG_INTERFACE(sortbucketsspu);
#else
bool sortbucketsspu_Dependency(const sysDependency& dependency);
#endif

AddActiveEntitiesData g_AddActiveEntitiesData ;
ASSERT_ONLY(u32 g_AddActiveEntitiesDataInUse = 0;)

#if !__FINAL
const char *CStreamingBucketSet::GetStreamingBucketName(StreamBucket bucketId)
{
	return s_StreamingBucketNames[bucketId];
}
#endif // !__FINAL


int CStreamingBucketSet::SortFuncQSort(const BucketEntry *a, const BucketEntry *b)
{
	// The values are guaranteed to be positive, so let's use int math instead.
	// It's faster.
	u32 scoreA = *((u32 *) &a->m_Score);
	u32 scoreB = *((u32 *) &b->m_Score);

	if (scoreA == scoreB)
	{
		return 0;
	}

	return (scoreA > scoreB) ? 1 : -1;
}

class BucketEntrySort
{
public:
	bool operator()(const BucketEntry& left, const BucketEntry& right) const
	{
		// The values are guaranteed to be positive, so let's use int math instead.
		// It's faster.
		u32 leftScore = *((u32 *) &left.m_Score);
		u32 rightScore = *((u32 *) &right.m_Score);
		return (leftScore < rightScore);
	}
};

void CStreamingBucketSet::AddActiveEntitiesToBucketAndSort(const fwLink<ActiveEntity>* pStore, u32 storeSize, const fwLink<ActiveEntity>* pFirst, const fwLink<ActiveEntity>* pLast)
{
	Assert(!m_SortDependencyRunning);
	Assert(!g_AddActiveEntitiesDataInUse);

#if !__FINAL
	if(m_DumpAllEntities)
	{
		DumpAllEntitiesOnce();
		m_DumpAllEntities = 0;
	}
#endif

	Assert(pStore != NULL && storeSize > 0 && pFirst != NULL && pLast != NULL);

#if __PPU
	// @HACK - we need to pad the list with dummy entries so that the SPU job
	// can add new entries at a 16-byte boundary
	while( (m_EntityList.GetCount() * sizeof(BucketEntry)) % 16 != 0)
	{
		AddEntry(float(STREAMBUCKET_ACTIVE_PED), NULL, 0);
	}
#endif

#if __PPU
	m_SortDependency.m_Flags = sysDepFlag::ALLOC0 | sysDepFlag::INPUT4;
#else
	m_SortDependency.m_Flags = sysDepFlag::INPUT4;
#endif
	m_SortDependency.m_Params[1].m_AsPtr = &m_SortDependencyRunning;
	m_SortDependency.m_Params[2].m_AsPtr = m_EntityList.GetElements();
	m_SortDependency.m_Params[3].m_AsInt = m_EntityList.GetCount();
	m_SortDependency.m_Params[4].m_AsPtr = &g_AddActiveEntitiesData;
#if USE_PAGED_POOLS_FOR_STREAMING
	m_SortDependency.m_Params[5].m_AsPtr = &strStreamingEngine::GetInfo().GetStreamingInfoPool().GetPageHeader(0);
#else // USE_PAGED_POOLS_FOR_STREAMING
	m_SortDependency.m_Params[5].m_AsPtr = strStreamingEngine::GetInfo().GetStreamingInfo(strIndex(0));
#endif // USE_PAGED_POOLS_FOR_STREAMING
	m_SortDependency.m_Params[6].m_AsInt = m_EntityList.GetCapacity();
	m_SortDependency.m_Params[7].m_AsInt = storeSize;

	g_AddActiveEntitiesData.m_EntityListCountPtr = m_EntityList.GetCountPointer();
	g_AddActiveEntitiesData.m_pStore = (void*)pStore;
	g_AddActiveEntitiesData.m_pFirst = (void*)pFirst;
	g_AddActiveEntitiesData.m_pLast = (void*)pLast;
	g_AddActiveEntitiesData.m_frameCount = TIME.GetFrameCount();
#if !__FINAL
	g_AddActiveEntitiesData.m_DumpAllEntitiesPtr = &m_DumpAllEntities;
#endif
#if __BANK
	g_AddActiveEntitiesData.m_BucketCounts = m_BucketCounts.GetElements();
#endif
#if __ASSERT
	g_AddActiveEntitiesData.m_bIsAnyVolumeActive = CStreamVolumeMgr::IsAnyVolumeActive();
	g_AddActiveEntitiesData.m_DataInUsePtr = &g_AddActiveEntitiesDataInUse;

	g_AddActiveEntitiesDataInUse = 1;
#endif

	m_SortDependency.m_DataSizes[0] = storeSize + BucketEntry::BUCKET_ENTRY_BATCH_SIZE*sizeof(BucketEntry);
	m_SortDependency.m_DataSizes[4] = sizeof(g_AddActiveEntitiesData);

#if USE_PAGED_POOLS_FOR_STREAMING
	m_SortDependency.m_DataSizes[5] = STREAMING_INFO_MAX_PAGE_COUNT * sizeof(strStreamingInfoPageHeader);
	m_SortDependency.m_Flags |= sysDepFlag::INPUT5;
#endif // USE_PAGED_POOLS_FOR_STREAMING

	m_SortDependencyRunning = 1;

	sysDependencyScheduler::Insert( &m_SortDependency );
}

void CStreamingBucketSet::SortBuckets(bool forcePPU)
{
	Assert(!m_SortDependencyRunning);

	int elementCount = m_EntityList.GetCount();

	if (!forcePPU && sm_UseSPUSort)
	{
		if (elementCount)
		{
#if __PPU
			m_SortDependency.m_Flags = sysDepFlag::ALLOC0;
#else
			m_SortDependency.m_Flags = 0;
#endif
			m_SortDependency.m_Params[1].m_AsPtr = &m_SortDependencyRunning;
			m_SortDependency.m_Params[2].m_AsPtr = m_EntityList.GetElements();
			m_SortDependency.m_Params[3].m_AsInt = elementCount;
			m_SortDependency.m_Params[4].m_AsPtr = NULL;
			m_SortDependency.m_DataSizes[0] = (((elementCount * sizeof(BucketEntry))+15)&~15);

			m_SortDependencyRunning = 1;

			sysDependencyScheduler::Insert( &m_SortDependency );
		}
	}
	else
	{
		if (sm_UseQSort)
		{
			m_EntityList.QSort(0, -1, SortFuncQSort);
		}
		else
		{
			BucketEntry *entries = m_EntityList.GetElements();

			std::sort(entries, entries + elementCount, BucketEntrySort());
		}
	}
}

void CStreamingBucketSet::WaitForSorting()
{
	PF_FUNC(WaitForSPUSort);

	while(true)
	{
		volatile u32 *pSortRunning = &m_SortDependencyRunning;
		if(*pSortRunning == 0)
		{
			break;
		}

		sysIpcYield(PRIO_NORMAL);
	}

#if !__FINAL
	if(m_DumpAllEntities)
	{
		DumpAllEntitiesOnce();
		m_DumpAllEntities = 0;
	}
#endif

	// Uncomment the code below to make sure that the values are
	// sorted the way we expect them to.
/*
	int count = m_EntityList.GetCount();

	if (count)
	{
		float first = m_EntityList[0].m_Score;

		for (int x=1; x<count; x++)
		{
			float newValue = m_EntityList[x].m_Score;
			Assertf(newValue >= first, "ORDER: %.2f %.2f", newValue, first);
			first = newValue;
		}
	}
	*/
}

// Needs to be outside the function's scope, or else cosf() gets called every time.
// 2.0 because subtendedAngle is actually half angle
const float		ms_fCosOfScreenYawForPriorityConsideration	= cosf(5.0f / 2.0f * PI / 180.0f);
//const float		ms_fHighPriorityYawInRadians				= 10.0f * PI / 180.0f;

bool CStreamingBucketSet::IsEntityFarAway(float distance, const CEntity *entity, Vector3::Vector3Param vCamPos, Vector3::Vector3Param vCamDir)
{
//	const float		ms_fScreenYawForPriorityConsideration	= 5.0f;		//in degrees
	const float		ms_fHighPriorityYaw						= 10.0f;	//in degrees

	if(!entity->IsBaseFlagSet(fwEntity::LOW_PRIORITY_STREAM))
	{
		Vector3 vCentre;
		float fRadius = entity->GetBoundCentreAndRadius(vCentre);

		Vector3 vDir = vCentre - vCamPos;
		distance = vDir.Mag();

		vDir *= 1.0f / (distance+0.00001f); // normalize the direction
		float fDot = vDir.Dot(vCamDir);

		if(fDot < 0.0f)
		{
			// if true, sphere overlaps camera so high priority
			// otherwise it is behind us so forget it
			return (distance >= fRadius);
		}
		else
		{
			if (fRadius > 35.0f && entity->GetLodData().IsLod())
			{
				return false;
			}

			// we want the cos of the angle subtended by the sphere, which is adjacent/hypot. Adjacent is dist to sphere
			float fHypot = sqrtf(distance*distance+fRadius*fRadius);
			float fCosofApproxAngleSubtendedBySphere = (distance / fHypot);

			// if it subtends a small angle then ignore it (is it small on the screen?)
			if (fCosofApproxAngleSubtendedBySphere < ms_fCosOfScreenYawForPriorityConsideration)
			{
				float fSubtendedAngle = acosf(fCosofApproxAngleSubtendedBySphere);

				// does it overlap a central cone (is it far off the center of the screen?)
				if (acosf(fDot)-fSubtendedAngle<(ms_fHighPriorityYaw/180.0f)*PI)
				{
					return false;
				}
			}
		}
	}

	return true;
}

bool CStreamingBucketSet::IsNearbyInteriorShell(CGtaPvsEntry *entry, CEntity *entity)
{
	const u32 streamingVolumeVisMask = ( 1 << CGameScan::GetInteriorStreamVisibilityBit() );

	fwInteriorLocation interiorLoc = entity->GetInteriorLocation();
	if ( interiorLoc.IsValid() && !interiorLoc.IsAttachedToPortal() && interiorLoc.GetRoomIndex()==0 )
	{
		return ( entry->GetVisFlags() & streamingVolumeVisMask ) != 0;
	}
	return false;
}

void CActiveEntityList::Init()
{
	m_ActiveEntities.Init(CSceneStreamerList::MAX_OBJ_INSTANCES, 16);
}

void CActiveEntityList::Update()
{
}

void CActiveEntityList::Shutdown()
{
	m_ActiveEntities.Shutdown();
}

fwLink<ActiveEntity>* CActiveEntityList::AddEntity(fwEntity* pfwEntity)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE)); // Don't even think about it.
	sysCriticalSection cs(s_CriticalSection);					

	// don't add peds and vehicles to list
	FastAssert(!static_cast<CEntity*>(pfwEntity)->GetIsTypePed() && !static_cast<CEntity*>(pfwEntity)->GetIsTypeVehicle());

	// 1/27/14 - CT - we need an initial score for this entity. STREAMBUCKET_VISIBLE seems like the most appropriate thing atm.
	ActiveEntity activeEntity;
	activeEntity.SetEntity(pfwEntity, BucketEntry::CalculateEntityID(pfwEntity), float(STREAMBUCKET_VISIBLE));
	activeEntity.MarkVisible();

	fwLink<ActiveEntity>* pLink = m_ActiveEntities.Insert(activeEntity);

#if __BANK
	if (!pLink)
	{
		g_SceneStreamerMgr.GetStreamingLists().GetActiveEntityList().GetBucketSet().DumpAllEntitiesOnce();

		Displayf("======= Active entity linked list =====");

		fwLink<ActiveEntity>* pLink = m_ActiveEntities.GetFirst()->GetNext();
		int count = 0;

		strStreamingModule *module = fwArchetypeManager::GetStreamingModule();

		while (pLink != m_ActiveEntities.GetLast())
		{
			CEntity* entity = static_cast<CEntity*>(pLink->item.GetEntity());
			strLocalIndex objIndex = strLocalIndex(entity->GetModelIndex());
			strStreamingInfo &info = strStreamingEngine::GetInfo().GetStreamingInfoRef(module->GetStreamingIndex(objIndex));
			int refCount = module->GetNumRefs(objIndex);
			Displayf("%d: %s (%p), flags=%x, ref=%d, depCount=%d, last visible: %d", ++count, entity->GetModelName(), entity,
				info.GetFlags(), refCount, info.GetDependentCount(), pLink->item.GetTimeSinceLastVisible());
			pLink = pLink->GetNext();
		}

		Assertf(false, "Too many active entities - see TTY for list.");
	}
#endif // __BANK

#if __DEV
	if(g_show_streaming_on_vmap)
		static_cast<CEntity*>(pfwEntity)->DrawOnVectorMap(Color_white);
#endif

	return pLink;
}

void CActiveEntityList::RemoveEntity(fwLink<ActiveEntity>* pLink)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE)); // Don't even think about it.
	sysCriticalSection cs(s_CriticalSection);

	if(Likely(pLink))
	{
		m_ActiveEntities.Remove(pLink);
	}	
}

// ------------ New streaming system --------------

bool CStreamingBucketSet::sm_UseSPUSort = true;
bool CStreamingBucketSet::sm_UseQSort = false;

CStreamingBucketSet::CStreamingBucketSet()
: m_SortDependencyRunning(0)
#if __BANK
, m_Removed(0)
#endif // __BANK
#if !__FINAL
, m_DumpAllEntities(0)
#endif
{
#if __BANK
	sysMemSet(&m_BucketCounts[0], 0, sizeof(m_BucketCounts));
#endif // __BANK

#if __PPU
	m_SortDependency.Init( FRAG_SPU_CODE(sortbucketsspu), 0, 0 );
#else
	m_SortDependency.Init( sortbucketsspu_Dependency, 0, 0 );
#endif
	m_SortDependency.m_Priority = sysDependency::kPriorityMed;
}

void CStreamingBucketSet::Reset()
{
	// Make sure the SPU isn't still busy messing with this list.
	WaitForSorting();

	m_EntityList.Resize(0);

#if __BANK
	sysMemSet(&m_BucketCounts[0], 0, sizeof(m_BucketCounts));
#endif // __BANK
}

#if !__FINAL
void CStreamingBucketSet::DumpAllEntitiesOnce()
{
	static bool firstDump = true;

	if (firstDump)
	{
		firstDump = false;
		Errorf("Streaming bucket overrun. List of all entities:");

		for (int x=0; x<m_EntityList.GetCount(); x++)
		{
			BucketEntry &entry = m_EntityList[x];
			CEntity *entity = entry.GetEntity();
			if(entry.IsStillValid())
			{
				const char *name = (entity) ? entity->GetModelName() : "[null]";
				const fwTransform &transform = entity->GetTransform();
				float xPos = transform.GetPosition().GetXf();
				float yPos = transform.GetPosition().GetYf();
				float zPos = transform.GetPosition().GetZf();

				Errorf("\t%p:\t%s:\tScore %.2f\t%.2f\t%.2f\t%.2f", entity, name, entry.m_Score, xPos, yPos, zPos);
			}
			else
			{
				Errorf("\tInvalid entity %p", entity);
			}
		}

#if __BANK
		RENDERPHASEMGR.ShowScanResults(false);
#endif // __BANK

		g_SceneStreamerMgr.DumpStreamingInterests();
		g_SceneStreamerMgr.DumpPvsAndBucketStats();
	}
}
#endif // !__FINAL

#if __BANK
void CStreamingBucketSet::ShowBucketUsage(bool showScoreMinMax)
{
	BucketEntry *entry = m_EntityList.GetElements();

	if (showScoreMinMax)
	{
		WaitForSorting();
	}

	for (int x=0; x<STREAMBUCKET_COUNT; x++)
	{
		float minScore = 0.0f;
		float maxScore = 0.0f;

		int bucketCount = m_BucketCounts[x];

		if (showScoreMinMax && bucketCount)
		{
			minScore = maxScore = entry->m_Score;
			entry++;

			Assertf((int) minScore == x, "Expected score %f to belong to bucket %d", minScore, x);
			ASSERT_ONLY(float prevScore = minScore; )

			for (int y=1; y<bucketCount; y++)
			{
				float score = entry->m_Score;
				maxScore = Max(maxScore, score);
				entry++;

				Assertf(score >= prevScore, "Unsorted bucket score list (%f vs %f)", score, prevScore);
				ASSERT_ONLY(prevScore = score;)
			}
		}

		char scoreInfo[64];

		if (showScoreMinMax)
		{
			formatf(scoreInfo, " Score: %f - %f", minScore, maxScore);
		}
		else
		{
			scoreInfo[0] = 0;
		}
		
		grcDebugDraw::AddDebugOutputEx(false, "%25s: %-4d (D: %4d/%4d) (R: %4d)%s", s_StreamingBucketNames[x], bucketCount,
			m_DeletionThisFrame[x], m_TotalDeletion[x], m_RequestsThisFrame[x], scoreInfo);
	}
}

void CStreamingBucketSet::ShowBucketContents(int bucketOnly)
{
	int count = m_EntityList.GetCount();
	int lastBucket = -1;

	for (int x=0; x<count; x++)
	{
		BucketEntry &entry = m_EntityList[x];
		float score = entry.m_Score;
		CEntity *entity = entry.GetEntity();

		int bucket = (int) score;

		if (bucketOnly == -1 || bucket == bucketOnly)
		{
			if (bucket != lastBucket)
			{
				lastBucket = bucket;
				grcDebugDraw::AddDebugOutputEx(false, "%25s: %-4d (D: %4d/%4d) (R: %4d)", s_StreamingBucketNames[bucket], m_BucketCounts[bucket],
					m_DeletionThisFrame[bucket], m_TotalDeletion[bucket], m_RequestsThisFrame[bucket]);
			}

			grcDebugDraw::AddDebugOutputEx(false, " %25s: %8.5f", entity->GetModelName(), score);
		}
	}
}

void CStreamingBucketSet::ShowRemovalStats()
{
	grcDebugDraw::AddDebugOutputEx(false, "Removed this frame: %d", m_Removed);
	grcDebugDraw::AddDebugOutputEx(false, "Average removed: %.5f", m_AverageRemoved);
}

void CStreamingBucketSet::UpdateStats()
{
	m_AverageRemoved = m_AverageRemoved * 0.95f + float(m_Removed) * 0.05f;
	m_Removed = 0;
	sysMemSet(&m_DeletionThisFrame, 0, sizeof(m_DeletionThisFrame));
	sysMemSet(&m_RequestsThisFrame, 0, sizeof(m_RequestsThisFrame));
}
#endif // __BANK



CStreamingLists::CStreamingLists()
: m_EntitiesToCreateDrawables(),
m_Timestamp(0), 
m_MasterCutoff(0.0),
m_ReadAheadFactor(0.5f),
m_IsDriveModeEnabled(false),
m_IsInteriorDriveModeEnabled(false),
m_CurrentInteriorProxyId(-1),
m_StreamMode(0),
m_CurrentRoomId(-1),
m_CameraPositionForFade(),
m_CameraDirection()
#if __BANK
, m_ShowBucketUsage(false)
, m_ShowScoreMinMax(false)
, m_ShowActiveBucketContents(false)
, m_ShowNeededBucketContents(false)
, m_DisplayOnlyBucket(-1)
, m_ShowRemovalStats(false)
, m_SPUSortForRequests(false)
, m_VisibilitySets(0)
, m_AddNearbyInteriors(true)
, m_AddStreamVolumes(true)
, m_AddRenderPhasePvs(true)
#endif // __BANK
{
}

void CStreamingLists::Init()
{
	m_ActiveEntityList.Init();

	m_ActiveEntityList.GetBucketSet().InitArray(MAX_ACTIVE_ENTITIES);
	m_NeededEntityList.GetBucketSet().InitArray(MAX_NEEDED_ENTITIES);

	m_MasterCutoff = 0.0f;
}

void CStreamingLists::Shutdown()
{
	m_ActiveEntityList.Shutdown();
}

bool CStreamingLists::IsRequired(const CEntity *entity)
{
	return !entity->GetIsTypeComposite() && !entity->GetIsTypeLight()

#if KEEP_INSTANCELIST_ASSETS_RESIDENT && 0
		&& !entity->GetIsTypeGrassInstanceList()	// exclude IP veg because the assets should be resident at all times
#endif

		&& !entity->GetIsTypeMLO() && !entity->GetIsTypePortal();
			
}

bool CStreamingLists::AddNeeded(CGtaPvsEntry *pvsEntry, CEntity *pEntity, const fwVisibilityFlags& visFlags, const bool isVisibleInGBufOrMirror, u32 streamMode, bool BANK_ONLY(skipVisFlags))
{
	Assert(!m_ListLocked);

	CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
	if (!pModelInfo)
	{
		return false;
	}
	Assert(!pModelInfo->GetHasLoaded());

#if __BANK
	// Don't add anything that isn't in any of the sets enabled via RAG.
	if (!skipVisFlags && !(visFlags & m_VisibilitySets))
	{
		return false;
	}
#endif // __BANK

	bool bRequired = IsRequired(pEntity) && !pModelInfo->IsModelMissing();
	if(!bRequired)
	{
		return false;
	}

#if __BANK
	if (CSceneStreamerDebug::DrawRequiredOnVmap())
	{
		fwVectorMap::Draw3DBoundingBoxOnVMap(pEntity->GetBoundBox(),
			g_SceneStreamerDebug.IsHighPriorityEntity(pEntity) ? Color_red : Color_green);
	}
#endif	//__BANK

	float bucketScore = CStreamingBucketSet::IdentifyBucket(pvsEntry, pEntity, visFlags, isVisibleInGBufOrMirror, m_CurrentInteriorProxyId, m_CurrentRoomId, m_CameraPositionForFade, m_CameraDirection, streamMode);
	m_NeededEntityList.GetBucketSet().AddEntry(bucketScore, pEntity, BucketEntry::CalculateEntityID(pEntity));

	return true;
}

void CStreamingLists::UpdateInteriorAndCamera()
{
	Vector3 dir = CGameWorld::FindFollowPlayerSpeed();
	float flipFactor = 1.0f;

	camGameplayDirector* gameplayDirector = camInterface::GetGameplayDirectorSafe();
	if(gameplayDirector && gameplayDirector->IsLookingBehind() && camInterface::IsRenderingDirector(*gameplayDirector) && g_ContinuityMgr.HighSpeedFlying())
	{
		// While flying:
		// The user pressed R3, so we're looking the opposite way, but probably still moving in the same way, so
		// let's change the scoring to look where we're going.
		flipFactor = -1.0f;
	}

	m_CameraPositionForFade = g_SceneToGBufferPhaseNew->GetCameraPositionForFade() + (dir * m_ReadAheadFactor);
	m_CameraDirection = g_SceneToGBufferPhaseNew->GetCameraDirection() * flipFactor;

	m_CurrentInteriorProxyId = -1;
	m_CurrentRoomId = -1;

	CViewport* pVp = gVpMan.GetGameViewport();
	if(pVp)
	{
		CInteriorProxy* pInteriorProxy = pVp->GetInteriorProxy();
		CInteriorInst* pInterior = (pInteriorProxy==NULL) ? NULL : pInteriorProxy->GetInteriorInst();

		if(pInterior && CInteriorInst::GetPool()->IsValidPtr(pInterior))
		{
			m_CurrentInteriorProxyId = CInteriorProxy::GetPool()->GetJustIndex(pInteriorProxy);
			m_CurrentRoomId = pVp->GetRoomIdx();
		}
	}
}

void CStreamingLists::CreateDrawablesForEntities()
{
	PF_AUTO_PUSH_TIMEBAR("CreateDrawablesForEntities");

	if(Likely(!m_EntitiesToCreateDrawables.IsFull()))
	{
		int numEntitiesToCreateDrawables = m_EntitiesToCreateDrawables.GetCount();
		for(int i = 0; i < numEntitiesToCreateDrawables; i++)
		{
			CEntity* entity = m_EntitiesToCreateDrawables[i];
			Assert(entity && entity->GetBaseModelInfo());

			entity->CreateDrawable();
		}
	}
	else
	{
		CPostScan::WaitForSortPVSDependency();

		PF_AUTO_PUSH_TIMEBAR("Full loop over PVS");

		// Our short, optimized list was full, so we might have missed some entities. 
		// This should happen very, very rarely - possibly only on loading/teleporting, 
		// etc. In this case, we'll take the hit to simply loop over the whole PVS, 
		// looking for drawables that need to be created.
		CGtaPvsEntry* stop = gPostScan.GetPVSEnd();
		for(CGtaPvsEntry* currentEntry = gPostScan.GetPVSBase(); currentEntry != stop; currentEntry++)
		{
			PrefetchDC(currentEntry + 1);

			CEntity* pEntity = currentEntry->GetEntity();
			if(!pEntity || pEntity->GetIsTypePed() || pEntity->GetIsTypeVehicle())
			{
				continue;
			}
#if __BANK
			// Don't add anything that isn't in any of the sets enabled via RAG.
			if (!(currentEntry->GetVisFlags() & m_VisibilitySets))
			{
				continue;
			}
#endif // __BANK
			
			CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
			if(!pEntity->GetDrawHandlerPtr() && pModelInfo && pModelInfo->GetHasLoaded())
			{
				pEntity->CreateDrawable();
			}
		}
	}

	// Finally, reset the count on the array for next frame.
	m_EntitiesToCreateDrawables.SetCount(0);
}

void CStreamingLists::PostGenerateLists()
{
#if __STATS
	PF_SET(ActiveEntities, m_ActiveEntityList.GetBucketSet().GetEntryCount());
	PF_SET(NeededEntities, m_NeededEntityList.GetBucketSet().GetEntryCount());
#endif // __STATS

	// Now each bucket needs to be sorted.
	PF_START(SortBuckets);
	const fwLinkList<ActiveEntity>& drawableList = m_ActiveEntityList.GetDrawableLinkList();
	const u32 storeSize = sizeof(fwLink<ActiveEntity>) * CSceneStreamerList::MAX_OBJ_INSTANCES;
	m_ActiveEntityList.GetBucketSet().AddActiveEntitiesToBucketAndSort(drawableList.GetStorePtr(), storeSize, drawableList.GetFirst()->GetNext(), drawableList.GetLast());
	m_NeededEntityList.GetBucketSet().SortBuckets(true BANK_ONLY(&& !m_SPUSortForRequests));
	PF_STOP(SortBuckets);	
	

#if __BANK
	if (m_ShowBucketUsage)
	{
		ShowBucketUsage();

		grcDebugDraw::AddDebugOutputEx(false, Color_white, "Last cutoff: %f", m_Cutoff);
		grcDebugDraw::AddDebugOutputEx(false, Color_white, "Master cutoff: %f", m_MasterCutoff);
	}

	ShowBucketContents();

	if (m_ShowRemovalStats)
	{
		ShowRemovalStats();
	}

	m_ActiveEntityList.GetBucketSet().UpdateStats();
	m_NeededEntityList.GetBucketSet().UpdateStats();
#endif // __BANK

	ASSERT_ONLY(m_ListLocked = true);
}

void CStreamingLists::PreGenerateLists()
{
#if STREAMING_VISUALIZE
//	UpdateVisibilityToStreamingVisualize();
#endif // STREAMING_VISUALIZE

	ASSERT_ONLY(m_ListLocked = false);

	m_Timestamp++;

	m_ActiveEntityList.GetBucketSet().Reset();
	m_NeededEntityList.GetBucketSet().Reset();

#if __BANK
	UpdateVisibilitySets();
#endif // __BANK
}

void CStreamingLists::WaitForSort()
{
	m_ActiveEntityList.GetBucketSet().WaitForSorting();
	m_NeededEntityList.GetBucketSet().WaitForSorting();
}

void CStreamingLists::AddAdditionalEntity(CEntity *pEntity, bool skipVisCheck, u32 streamMode)
{
	// Is it already active?
	if (pEntity->GetDrawHandlerPtr())
	{
		// If so, then the primary pass already added it.
		// TODO: However, if it wasn't already in the PVS, we should bump its score up to something
		// higher so it won't get deleted prematurely.
		return;
	}

	// If it is not in the active list, but its model has loaded, 
	// then no reason to add it to the active list.
	CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
	if(pModelInfo && pModelInfo->GetHasLoaded())
	{
		return;
	}

	// Create a dummy PVS entry.
	CGtaPvsEntry pvsEntry;
	fwVisibilityFlags flags;

	flags.Clear();

	pvsEntry.SetEntity(pEntity);
	pvsEntry.SetDist(100.0f);		// TODO: Random dummy value, maybe we can find something better here.
	pvsEntry.SetVisFlags(flags);	// TODO: Random dummy values, maybe we can find something better here.

	// TODO: Don't add it twice!
	AddNeeded(&pvsEntry, pEntity, flags, false, streamMode, skipVisCheck);
}

void CStreamingLists::AddFromMapGroupSwap()
{
	u32 streamMode = m_StreamMode;

	atArray<fwEntity*> imapGroupSwapEntities;
	fwImapGroupMgr& imapGroupMgr = INSTANCE_STORE.GetGroupSwapper();

	// get required entities
	for (u32 i=0; i<fwImapGroupMgr::MAX_CONCURRENT_SWAPS; i++)
	{
		if (imapGroupMgr.GetIsActive(i) && imapGroupMgr.GetIsSwapImapLoaded(i))
		{
			imapGroupMgr.GetSwapEntities(i, imapGroupSwapEntities);
		}
	}

	// process required entities for active imap group swaps
	if (imapGroupSwapEntities.GetCount()>0)
	{
		int entityCount = imapGroupSwapEntities.GetCount();
		for (s32 i=0; i<entityCount; i++)
		{
			CEntity* pEntity = (CEntity*) imapGroupSwapEntities[i];
			if (pEntity)
			{
				AddAdditionalEntity(pEntity, true, streamMode);
			}
		}
	}
}

void CStreamingLists::AddFromInteriorRoomZero(CInteriorInst* pIntInst)
{
	if (pIntInst->GetProxy()->GetCurrentState() < CInteriorProxy::PS_FULL){
		return;
	}

	u32 streamMode = m_StreamMode;

	u32 numObj = pIntInst->GetNumObjInRoom(0);
	for(u32 i=0; i<numObj; i++)
	{
		CEntity* pEntity = pIntInst->GetObjInstancePtr(0, i);
		if(pEntity && (pEntity!=pIntInst->GetInternalLOD()) && !pEntity->GetIsTypeLight()) //don't request streaming of light objects
		{
			AddAdditionalEntity(pEntity, true, streamMode);
		}
	}
}

fwPool<CInteriorInst>*	pDebugPool = NULL;
fwPool<CBuilding>*		pDebugBuildingPool = NULL;
CInteriorInst*			debugInterior = NULL;
s32						debugSlot = -1;

void CStreamingLists::AddFromRequiredInteriors()
{
	CPed* pPlayer = CPedFactory::GetFactory()->GetLocalPlayer();
	CInteriorInst*	pPlayerInterior = pPlayer ? pPlayer->GetPortalTracker()->GetInteriorInst() : NULL;
	CInteriorInst* pIntInst = NULL;

	pDebugBuildingPool = CBuilding::GetPool();
	pDebugPool = CInteriorInst::GetPool();

	pIntInst = CInteriorInst::GetPool()->GetSlot(0);

	s32 poolSize=CInteriorInst::GetPool()->GetSize();
	s32 i = 0;
	while(i<poolSize)
	{
		debugSlot = i;
		pIntInst = CInteriorInst::GetPool()->GetSlot(i++);
		debugInterior = pIntInst;
		if (pIntInst && pIntInst->CanReceiveObjects() && (pIntInst != pPlayerInterior))
		{
			if ((pIntInst->GetGroupId() > 1) && pIntInst->CanReceiveObjects())
			{
				// add any active short tunnel interiors
				AddFromInteriorRoomZero(pIntInst);
			} 
			else if ((pIntInst->GetGroupId() == 1) && pPlayerInterior && (pPlayerInterior->GetGroupId() == 1))
			{
				// add any active metro interior, but only if the player is in the metro
				AddFromInteriorRoomZero(pIntInst);
			}
		}
	}
}


void CStreamingLists::AddFromStreamingVolume()
{
	u32 streamMode = m_StreamMode;

	// special case just for player switching - line seg volumes aren't supported
	// in the visibility system so we just extract them from the map data directly
	for (s32 i=0; i<CStreamVolumeMgr::VOLUME_SLOT_TOTAL; i++)
	{
		CStreamVolume& volume = CStreamVolumeMgr::GetVolume(i);
		if ( volume.IsActive() && volume.IsTypeLine() )
		{
			atArray<CEntity*> entityList;
			volume.ExtractEntitiesFromMapData(entityList);

			for (s32 i=0; i<entityList.GetCount(); i++)
			{
				CEntity* pEntity = entityList[i];
				if (pEntity)
				{
					AddAdditionalEntity(pEntity, true, streamMode);
				}
			}
		}
	}
}

#if STREAMING_VISUALIZE
/*
void CStreamingLists::UpdateVisibilityToStreamingVisualize()
{
	if (strStreamingVisualize::IsInstantiated())
	{
		WaitForSort();

		USE_DEBUG_MEMORY();

		// Everything that was removed.
		int newRemovedCount = s_RemovedEntitiesForSV.GetCount();

		for (int x=0; x<newRemovedCount; x++)
		{
			s_NowInvisibleForSV.Grow(1024) = s_RemovedEntitiesForSV[x];
			s_ActiveEntitiesForSV.Delete(s_RemovedEntitiesForSV[x]);
		}

		// And finally, let's see what's visible and what's not.
		BucketEntry *actPtr = GetActiveEntityList().GetBucketSet().GetEntries();
		int actCount = GetActiveEntityList().GetBucketSet().GetEntryCount();
		BucketEntry *actEnd = GetActiveEntityList().GetBucketSet().GetEntries() + actCount;

		// First - all those that are not visible.
		while (actPtr < actEnd)
		{
			if (actPtr->m_Score >= (float)STREAMBUCKET_VEHICLESANDPEDS_CUTSCENE)
			{
				// We're done with off-screen ones.
				break;
			}

			fwEntity *entity = actPtr->GetEntity();
			if (s_ActiveEntitiesForSV[entity])
			{
				// State change - this is no longer visible.
				s_ActiveEntitiesForSV[entity] = false;
				s_NowInvisibleForSV.Grow(1024) = entity;
			}

			actPtr++;
		}

		// Now those that are visible.
		while (actPtr < actEnd)
		{
			fwEntity *entity = actPtr->GetEntity();
			if (!s_ActiveEntitiesForSV[entity])
			{
				// State change - this is visible.
				s_ActiveEntitiesForSV[entity] = true;
				s_NewEntitiesForSV.Grow(1024) = entity;
			}

			actPtr++;
		}

		// Let's see what's no longer visible.
		STRVIS_UPDATE_VISIBILITY(s_NewEntitiesForSV.GetElements(), s_NewEntitiesForSV.GetCount(), s_NowInvisibleForSV.GetElements(), s_NowInvisibleForSV.GetCount());

		s_NewEntitiesForSV.Resize(0);
		s_NowInvisibleForSV.Resize(0);
		s_RemovedEntitiesForSV.Resize(0);
	}	
}

void CStreamingLists::AddActiveForStreamingVisualize(const fwEntity *entity)
{
	if (strStreamingVisualize::IsInstantiated())
	{
		USE_DEBUG_MEMORY();

		s_NewEntitiesForSV.Grow(1024) = entity;
	}
}

void CStreamingLists::RemoveActiveForStreamingVisualize(const fwEntity *entity)
{
	if (strStreamingVisualize::IsInstantiated())
	{
		USE_DEBUG_MEMORY();

		s_RemovedEntitiesForSV.Grow(1024) = entity;
	}
}
*/
#endif // STREAMING_VISUALIZE



#if __BANK
void CStreamingLists::AddWidgets(bkGroup &group)
{
	group.AddToggle("Show Bucket Usage", &m_ShowBucketUsage);
	group.AddToggle("Show Min/Max Scores in Buckets", &m_ShowScoreMinMax);
	group.AddToggle("Show Active Bucket Contents", &m_ShowActiveBucketContents);
	group.AddToggle("Show Needed Bucket Contents", &m_ShowNeededBucketContents);
	group.AddSlider("Show One Bucket Only", &m_DisplayOnlyBucket, -1, STREAMBUCKET_COUNT - 1, 1);
	group.AddToggle("Show Removal Stats", &m_ShowRemovalStats);

	group.AddToggle("Use SPU Sort", CStreamingBucketSet::GetUseSPUSortPtr());
	group.AddToggle("Use QSort", CStreamingBucketSet::GetUseQSortPtr());
	group.AddToggle("Use SPU Sort For Requests", &m_SPUSortForRequests);

	// Must be called after CGameScan::Init()
	Assert(CGameScan::GetStreamVolumeVisibilityBit1() != 0);
	Assert(CGameScan::GetStreamVolumeVisibilityBit2() != 0);

	bkGroup *visSetGroup = group.AddGroup("Visibility Sets");
	visSetGroup->AddToggle("Visible for Render Phase", &m_AddRenderPhasePvs);
	visSetGroup->AddToggle("IMAP Group Swaps", g_SceneStreamerMgr.GetStreamMapSwapsPtr());
	visSetGroup->AddToggle("High Detail 80m Sphere", g_SceneStreamerMgr.GetStreamByProximityPtr());
	visSetGroup->AddToggle("Nearby Interiors", &m_AddNearbyInteriors);
	visSetGroup->AddToggle("Streaming Volumes", &m_AddStreamVolumes);
}

void CStreamingLists::SetSceneStreamingEnabled(bool bEnabled)
{
	m_AddRenderPhasePvs = bEnabled;
	m_AddNearbyInteriors = bEnabled;
	m_AddStreamVolumes = bEnabled;
	bool* pbMapSwaps = g_SceneStreamerMgr.GetStreamMapSwapsPtr();
	bool* pbProximity = g_SceneStreamerMgr.GetStreamByProximityPtr();
	*pbMapSwaps = bEnabled;
	*pbProximity = bEnabled;
}

void CStreamingLists::UpdateVisibilitySets()
{
	u32 hdStreamMask = 1 << CGameScan::GetHdStreamVisibilityBit();
	u32 nearbyInteriorMask = 1 << CGameScan::GetInteriorStreamVisibilityBit();
	u32 streamVolumeMask = CGameScan::GetStreamVolumeMask();

	m_VisibilitySets = ((1 << fwVisibilityFlags::MAX_PHASE_VISIBILITY_BITS)-1) & ~(hdStreamMask | nearbyInteriorMask | streamVolumeMask);

	if (!m_AddRenderPhasePvs)
		m_VisibilitySets = 0;
	
	if (*g_SceneStreamerMgr.GetStreamByProximityPtr())
		m_VisibilitySets |= hdStreamMask;

	if (m_AddNearbyInteriors)
		m_VisibilitySets |= nearbyInteriorMask;

	if (m_AddStreamVolumes)
		m_VisibilitySets |= streamVolumeMask;
}

void CStreamingLists::ShowBucketUsage()
{
	grcDebugDraw::AddDebugOutputEx(false, Color_white, "Active List");
	m_ActiveEntityList.GetBucketSet().ShowBucketUsage(m_ShowScoreMinMax);
	grcDebugDraw::AddDebugOutputEx(false, Color_white, "Needed List");
	m_NeededEntityList.GetBucketSet().ShowBucketUsage(m_ShowScoreMinMax);
}

void CStreamingLists::ShowBucketContents()
{
	if (m_ShowActiveBucketContents)
	{
		CPostScan::WaitForSortPVSDependency();
		grcDebugDraw::AddDebugOutputEx(false, Color_white, "Active List");
		m_ActiveEntityList.GetBucketSet().ShowBucketContents(m_DisplayOnlyBucket);
	}

	if (m_ShowNeededBucketContents)
	{
		CPostScan::WaitForSortPVSDependency();
		grcDebugDraw::AddDebugOutputEx(false, Color_white, "Needed List");
		m_NeededEntityList.GetBucketSet().ShowBucketContents(m_DisplayOnlyBucket);
	}
}

void CStreamingLists::ShowRemovalStats()
{
	grcDebugDraw::AddDebugOutputEx(false, Color_white, "Active List");
	m_ActiveEntityList.GetBucketSet().ShowRemovalStats();
	grcDebugDraw::AddDebugOutputEx(false, Color_white, "Needed List");
	m_NeededEntityList.GetBucketSet().ShowRemovalStats();
}
#endif // __BANK


