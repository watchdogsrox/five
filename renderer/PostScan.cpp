// Title	:	PostScan.cpp
// Author	:	John Whyte
// Started	:	4/8/2009

// header stuff
#include "renderer/PostScan.h"

// Framework headers
#include "fwanimation/directorcomponentmotiontree.h"
#include "fwrenderer/RenderListBuilder.h"
#include "fwrenderer/renderlistgroup.h"
#include "fwscene/scan/Scan.h"
#include "fwscene/scan/ScanDebug.h"
#include "fwscene/world/WorldRepMulti.h"
#include "streaming/streamingvisualize.h"

// Rage headers
#include "profile/element.h"
#include "profile/group.h"
#include "profile/page.h"
#include "profile/timebars.h"
#include "system/membarrier.h"
#include "system/spinlockedobject.h"

// game includes
#include "camera/camInterface.h"
#include "camera/base/BaseCamera.h"
#include "camera/debug/DebugDirector.h"
#include "camera/viewports/ViewportManager.h"

#include "control/replay/replay.h"
#include "cutscene/CutSceneManagerNew.h"
#include "debug/Rendering/DebugLighting.h"
#include "debug/Rendering/DebugLights.h"
#include "objects/objectpopulation.h"
#include "objects/DummyObject.h"
#include "modelinfo/MloModelInfo.h"
#include "modelinfo/TimeModelInfo.h"
#include "peds/ped.h"
#include "renderer/Mirrors.h"
#include "renderer/RenderPhases/RenderPhaseParaboloidShadows.h"
#include "renderer/RenderPhases/RenderPhaseWaterReflection.h"
#include "renderer/RenderPhases/RenderPhaseReflection.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "renderer/Renderphases/RenderPhaseMirrorReflection.h"
#include "renderer/shadows/shadows.h"
#include "renderer/Lights/LightEntity.h"
#include "renderer/Lights/LODLights.h"
#include "renderer/renderListGroup.h"
#include "scene/lod/LodMgr.h"
#include "scene/portals/Portal.h"
#include "scene/portals/PortalInst.h"
#include "scene/streamer/SceneStreamerList.h"
#include "scene/streamer/SceneStreamerMgr.h"
#include "scene/streamer/StreamVolume.h"
#include "scene/texLod.h"
#include "scene/world/GameScan.h"
#include "scene/world/GameWorld.h"
#include "scene/AnimatedBuilding.h"
#include "scene/DynamicEntity.h"
#include "streaming/streaming.h"
#include "streaming/streamingdebug.h"
#include "streaming/streamingrequestlist.h"
#include "timecycle/TimeCycleAsyncQueryMgr.h"
#include "vfx/misc/DistantLights.h"
#include "control/replay/ReplayLightingManager.h"

#if __BANK
#include "debug/GtaPicker.h"
#include "debug/TiledScreenCapture.h"
#include "scene/debug/PostScanDebug.h"
#include "scene/lod/LodDebug.h"
#include "scene/world/GameScan.h"
#include "fwscene/world/InteriorSceneGraphNode.h"
#include "fwscene/world/SceneGraphVisitor.h"
#endif	//__BANK

#define PRERENDER_VEHICLE_SPHERE_RADIUS				(50)
#define PRERENDER_PED_AND_OBJECT_SPHERE_RADIUS		(20)
#define ALPHA_CULL_THRESHOLD						(50)

#if __DEV
#define INVALID_PVS_ENTRY_PATTERN					0xDF
#define INVALID_PVS_ENTRY_POINTER_VALUE				0xDFDFDFDF
#endif

#define __CHECK_FOR_DUPLICATES 0 //__DEV

RENDER_OPTIMISATIONS()
SCENE_OPTIMISATIONS()

// init
CPostScan	gPostScan;
CPostScanExteriorHdCull g_PostScanCull;

static fwSearch ms_PreRenderVehicleSearch(150);
static fwSearch ms_PreRenderPedAndObjectSearch(1024);

#define MAX_ALPHA_OVERRIDES (256)

struct alphaOverride {
	RegdEnt	entity;
	int alpha;

#if __BANK
	atString owner;
#endif
	};
static alphaOverride ms_alphaOverrides[MAX_ALPHA_OVERRIDES];
static bool ms_hasAnyAlphaOverride = false;
static int ms_lastAlphaOverride = 0;

struct entitySortBias {
	RegdEnt entity;
	float bias;
};
static atArray<entitySortBias> ms_sortBiases;

bool BuildRenderListsFromPVS_Dependency(const sysDependency& dependency);
bool SortPVSAsync_Dependency(const sysDependency& dependency);

sysDependency	g_BuildRenderListsFromPVSDependency;
sysDependency	g_SortPVSDependency;
u32				g_BuildRenderListsFromPVSDependencyRunning;
u32				g_SortPVSDependencyRunning;

bool CPostScan::ms_bAsyncSearchStarted = false;
bool CPostScan::ms_bAllowLateLightEntities = false;
spdSphere  CPostScan::ms_LateLightsNearCameraSphere;
atArray< CEntity*> CPostScan::ms_lateLights;

void ForceUpLinkedInteriors(fwInteriorLocation intLoc, CInteriorInst *pIntInst, const spdTransposedPlaneSet8 &frustum, atArray<fwInteriorLocation> &visitedLocs);

//
// name:		CPostScan::CPostScan
// description:	CPostScan constructor
//
CPostScan::CPostScan()
{
	fwRenderListBuilder::SetFlushPrototypesCallback(CDrawListPrototypeManager::Flush);
	fwRenderListBuilder::EnableScissoringExterior(true);

	m_nonVisiblePreRenderList.Init(1024);

	ms_lateLights.Reset();

#if !__FINAL
	m_EntityCount = 0;
#endif

}

//
// name:		CPostScan::Init
// description:	Initialise CPostScan structures
//
void CPostScan::Init(unsigned /*initMode*/)
{
	// need to allocate this once code has all moved out of renderer.cpp
	//gRenderListGroup.Init();

#if __PPU
	// We're using a 64-bit DMA on PS3, so this pointer needs to be aligned accordingly.
	CompileTimeAssert((OffsetOf(CPostScan, m_ResultBlocks) & 0x7) == 0);
#endif // __PPU	
	sysMemSet( ms_alphaOverrides, 0, sizeof(alphaOverride) * MAX_ALPHA_OVERRIDES );
	ms_hasAnyAlphaOverride = false;
	ms_lastAlphaOverride = 0;
	//@@: location CPOSTSCAN_INIT
	g_SortPVSDependency.Init( SortPVSAsync_Dependency, 0, sysDepFlag::INPUT0 );
	g_SortPVSDependency.m_Priority = sysDependency::kPriorityCritical;

	g_BuildRenderListsFromPVSDependency.Init( BuildRenderListsFromPVS_Dependency, 0, 0 );
	g_BuildRenderListsFromPVSDependency.m_Priority = sysDependency::kPriorityLow;

	g_PostScanCull.Init();
}

void CPostScan::Shutdown(unsigned /*initMode*/)
{
}
namespace PostScanHelperStats
{
	PF_PAGE(PostScanHelperPage,"PostScanHelper");

	PF_GROUP(PostScanHelper);
	PF_LINK(PostScanHelperPage, PostScanHelper);
	PF_TIMER(BuildRenderLists, PostScanHelper);
	PF_TIMER(SortRenderLists, PostScanHelper);
}
using namespace PostScanHelperStats;

//
// name:		CPostScan::Reset
// description:	Reset the ptrs to the start of the buffers ready for new frame
//
void CPostScan::Reset()
{
#if 0
	sysMemSet( m_PVS, INVALID_PVS_ENTRY_PATTERN, sizeof(CGtaPvsEntry) * MAX_PVS_SIZE );
	sysMemSet( m_ResultBlocks, INVALID_PVS_ENTRY_PATTERN, sizeof(m_ResultBlocks) );
#endif
	m_pCurrPVSEntry = &m_PVS[0];
	m_NextResultBlock = 0;
	m_ResultBlockCount = 0;
}

void CPostScan::BuildRenderListsNew()
{
	//Performing the LOD Lights Bucket Visibility here as
	//we would have the updated scan results flag by this point
	PF_PUSH_TIMEBAR("LOD/Distant LOD Lights Bucket Visibility");
	CLODLightManager::BuildDistLODLightsVisibility(gVpMan.GetGameViewport());
	CLODLightManager::BuildLODLightsBucketVisibility(gVpMan.GetGameViewport());
	PF_POP_TIMEBAR();

	ProcessPVSForRenderPhase();	

	// handle all of the pre-rendering of entities which require it...
	CRenderer::PreRenderEntities();

#if __BANK
	CPostScanDebug::ProcessAfterAllPreRendering();
	DebugLights::UpdateLights();
#endif
}

#if SCL_LOGGING
bool g_forceWait = false;
#endif

__forceinline void StartPreRenderForEntity(CEntity * entity, const bool bIsVisibleInMainViewport = true)
{
	if ( entity &&
		entity->GetIsTypeLight() == false && 
		entity->IsBaseFlagSet( fwEntity::HAS_PRERENDER ) )
	{
		Assert(!entity->GetPreRenderedThisFrame());

		const bool	preRenderLater = !entity->GetDrawable() || (entity->GetAttachParent() != NULL);

		if ( preRenderLater )
			gRenderListGroup.GetPreRenderList().AddEntity( entity );
		else
		{
#if __BANK
			CPostScanDebug::ProcessPreRenderEntity(entity);
#endif

			const ePrerenderStatus	preRenderStatus = entity->PreRender(bIsVisibleInMainViewport);
			if ( preRenderStatus != PRERENDER_DONE )
				gRenderListGroup.GetPreRender2List().AddEntity( entity );

			gRenderListGroup.GetAlreadyPreRenderedList().AddEntity( entity );

			ASSERT_ONLY( entity->SetPreRenderedThisFrame(true); )
			BANK_ONLY( CPostScanDebug::GetNumModelsPreRenderedRef() += 1; )
		}
	}
}

bool CPostScan::RejectTimedEntity(CEntity* RESTRICT pEntity, const bool visibleInGbuf)
{
#if __BANK
	if ( gStreamingRequestList.IsRecording() && !gStreamingRequestList.IsSkipTimedEntities() )
	{
		return false;
	}
#endif // !__FINAL

	CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
	Assert(pModelInfo->GetModelType()==MI_TYPE_TIME);

	CTimeInfo* pTimeInfo = pModelInfo->GetTimeInfo();
	Assert(pTimeInfo);
	//u32 otherModel = pTimeInfo->GetOtherTimeModel();

	int game_hour = CClock::GetHour();
	int game_minute = CClock::GetMinute();

	if (!pTimeInfo->OnlySwapWhenOffScreen())
	{
		// allowed to switch on when visible so are probably lights so add some noise to
		// the time so all the buildings don't come on together
		const u32 noiseFactor = (( pEntity->GetLodData().IsOrphanHd() ? 0 : g_LodMgr.GetHierarchyNoiseFactor(pEntity) ) >> 2) & 31;

		game_minute += noiseFactor;

		while(game_minute > 60)
		{
			game_minute -= 60;
			game_hour++;
		}
		if(game_hour > 23)
		{
			game_hour = 0;
		}
	}

	if ( !visibleInGbuf || !pTimeInfo->OnlySwapWhenOffScreen() )
	{
		const bool	isOn = pTimeInfo->IsOn(game_hour);
		pEntity->SetTimedEntityDelayedState( isOn );
		return isOn == false;
	}
	else
	{
		return pEntity->GetTimedEntityDelayedState() == false;
	}

}

void CPostScan::ProcessResultBlock(const int blockIndex, const u32 viewportBits, const u32 lastVisibleFrame)
{
	fwScanResultBlock& resultBlock = m_ResultBlocks[blockIndex];

	CGtaPvsEntry* entry = (CGtaPvsEntry*) resultBlock.m_PvsEntry;
	CGtaPvsEntry* lastEntry = entry + resultBlock.m_EntryCount;

	Assertf(entry != (CGtaPvsEntry*)INVALID_PVS_ENTRY_POINTER_VALUE, "Entry %d isn't valid yet (at %p)", blockIndex, entry);

	CStreamingLists &lists = g_SceneStreamerMgr.GetStreamingLists();
	const u32 streamMode = lists.GetStreamMode();

	for (; entry != lastEntry; entry++)
	{
		CEntity* entity = reinterpret_cast<CEntity*>( entry->GetEntity() );

#if __BANK 
		// trace selected entity
 		if (CPostScanDebug::ms_breakIfEntityIsInPVS && entity == g_PickerManager.GetSelectedEntity())
 		{
 			__debugbreak();
 		}
		if (CPostScanDebug::ms_renderVisFlagsForSelectedEntity && g_PickerManager.GetSelectedEntity()) 
		{
			fwEntity *pSelectedEntity = g_PickerManager.GetSelectedEntity();
			if (pSelectedEntity)
			{
				if (entity == pSelectedEntity)
				{
					int index = TIME.GetFrameCount() & 1;
					CPostScanDebug::ms_pvsEntryForSelectedEntity[index] = *entry;
					CPostScanDebug::ms_pvsEntryEntityRenderPhaseVisibilityMask[index] = entry->GetEntity()->GetRenderPhaseVisibilityMask().GetAllFlags();
				}
			}
		}
#endif // __BANK

		const fwVisibilityFlags& visFlags = entry->GetVisFlags();
		CEntityDrawHandler* drawHandler = (CEntityDrawHandler*) entity->GetDrawHandlerPtr();
		PrefetchDC(drawHandler);

		const bool isVisibleInGBufOrMirror = (visFlags & viewportBits) != 0;
		
		// Prefetch the entity that is two entries ahead, since some entities get rejected 
		// fairly quickly by being a timed entity, etc.
		const CGtaPvsEntry* pNextEntryToPrefetch = entry + 2;
		if(Likely(pNextEntryToPrefetch < lastEntry))
		{
			PrefetchDC(pNextEntryToPrefetch->GetEntity());
		}

		// Prefetch the next cache line of PVS entries.
		PrefetchDC((char*)entry+128);

		Assertf(entity != (CEntity*)INVALID_PVS_ENTRY_POINTER_VALUE, "Invalid entity in block %d at %p", blockIndex, entry);
		if(Unlikely(!entity))
		{
			continue;
		}

		// If the entity is a timed entity, and isn't supposed to be visible at this time of day, 
		// then invalidate the visibility entry and continue.
		if (entity->IsTimedEntity() && this->RejectTimedEntity(entity, isVisibleInGBufOrMirror))
		{
			entry->Invalidate();

			entity->ReleaseImportantSlodAssets();

			// rare special case - timed emissive objects with lights attached should destroy their draw handler
			// when rejected, to avoid the light drawing before emissive model is loaded - B* 1337696 (IanK)
			if (entity->GetDrawHandlerPtr()!=NULL && entity->m_nFlags.bLightObject)
			{
				entity->DeleteDrawable();
			}
			continue;
		}

		lists.ProcessPvsEntry(entry, entity, visFlags, isVisibleInGBufOrMirror, drawHandler, lastVisibleFrame, streamMode);

		if(isVisibleInGBufOrMirror)
		{
			if(entity->GetIsDynamic())
			{
				static_cast<CDynamicEntity*>(entity)->SetIsVisibleInSomeViewportThisFrame(true);
			}

			StartPreRenderForEntity(entity);
		}
	}

}

void CPostScan::CleanAndSortPreRenderLists()
{
	PF_AUTO_PUSH_TIMEBAR("Clean and Sort PreRender Lists");

	fwEntityPreRenderList& alreadyPreRenderedList = gRenderListGroup.GetAlreadyPreRenderedList();
	fwEntityPreRenderList& preRenderList = gRenderListGroup.GetPreRenderList();

	alreadyPreRenderedList.SortAndRemoveDuplicates();
	preRenderList.SortAndRemoveDuplicates();
	m_nonVisiblePreRenderList.SortAndRemoveDuplicates();

	preRenderList.RemoveElements( alreadyPreRenderedList );

	// Now that we know the set of visible stuff that has been (or will be) pre-rendered, we can 
	// remove visible entities from the non-visible list, so that only non-visible entities remain.
	m_nonVisiblePreRenderList.RemoveElements( preRenderList );
	m_nonVisiblePreRenderList.RemoveElements( alreadyPreRenderedList );
}

void CPostScan::PreRenderNonVisibleEntities()
{
	PF_AUTO_PUSH_TIMEBAR("PreRender Non-Visible Entities");

	const int numEntitiesInNonVisiblePreRenderList = m_nonVisiblePreRenderList.GetCount();
	for(int i = 0; i < numEntitiesInNonVisiblePreRenderList; i++)
	{
		CEntity* pEntity = static_cast<CEntity*>( m_nonVisiblePreRenderList.GetEntity(i) );
		Assert( pEntity );
		if(pEntity->GetIsVisible())
		{
			StartPreRenderForEntity(pEntity, false);
		}
	}
}

bool PreRenderSearchCBVehicle(fwEntity* entity, void* spherePtr)
{
	CVehicle* pEntity = static_cast<CVehicle*>(entity);
	Assert(pEntity->GetIsDynamic() && pEntity->IsBaseFlagSet(fwEntity::HAS_PRERENDER));

	// this entity has already been PreRendered, so we can avoid the checks below
	if(pEntity->GetIsVisibleInSomeViewportThisFrame())
	{
		return true;
	}

	// this entity needs to be PreRendered (most likely because its lights are on)
	if(pEntity->ShouldPreRenderBeyondSmallRadius())
	{
		gPostScan.AddToNonVisiblePreRenderList(pEntity);
		return true;
	}
	
	// if this entity is within the smaller radius, then we need to PreRender it
	const spdSphere& nearCameraSphere = *(static_cast<const spdSphere*>(spherePtr));
	spdSphere entitySphere = pEntity->GetBoundSphere();
	if(nearCameraSphere.IntersectsSphere(entitySphere))
	{
		gPostScan.AddToNonVisiblePreRenderList(pEntity);
		return true;
	}

	// if we hit this point, then the entity does not need PreRendering
	return true;
}

bool PreRenderSearchCBNonVehicle(fwEntity* entity, void*)
{
	CEntity* pEntity = static_cast<CEntity*>(entity);

	// The only types we should be processing here are Peds, Vehicles, and Objects, all of 
	// which explicitly set the HAS_PRERENDER flag. StartPreRenderForEntity() has a real 
	// check for HAS_PRERENDER anyways, so it won't call PreRender() on anything without 
	// HAS_PRERENDER set.
	Assert(pEntity->GetIsDynamic() && pEntity->IsBaseFlagSet(fwEntity::HAS_PRERENDER));

	gPostScan.AddToNonVisiblePreRenderList(pEntity);
	return true;
}

void CPostScan::ProcessPvsWhileScanning()
{
	int			availableEntries = 0;
	int			currentEntryIndex = 0;
	const u32	viewportBits = CRenderer::GetSceneToGBufferListBits() | CRenderer::GetMirrorListBits();
	const u32	lastVisibleFrame = TIME.GetFrameCount();

	CDynamicEntity::NextFrameForVisibility();
	fwAnimDirectorComponentMotionTree::SetLockedGlobal(true, fwAnimDirectorComponent::kPhasePreRender);

#if SCL_LOGGING
	sysTimer logTimer;
	const float waitTime = 30.0f * 33.33333f; // 20 frames, probably less as we're not running at 30
	int runTwice = 0;
#endif
#if __BANK
	CPostScanDebug::ms_pvsEntryForSelectedEntity[TIME.GetFrameCount() & 1].SetEntity(NULL);
#endif
	// Process the PVS entries found by visibility scan jobs that have 
	// already completed. If we run out of PVS entries, but scan is not yet 
	// finished, wait for scan to produce more PVS entries and process them.
	PF_PUSH_TIMEBAR("Process PVS Entries");

	// Are we moving fast? Then switch to a different stream mode.
	// Completely arbitrary check to see if we're moving fast.
	CStreamingLists &lists = g_SceneStreamerMgr.GetStreamingLists();
	u32 streamMode = 0;
	Vector3 dir = CGameWorld::FindFollowPlayerSpeed();
	if (dir.Mag2() > 15.0f * 15.0f)
	{
		if (*lists.GetInteriorDriveModePtr())
		{
			streamMode |= CSceneStreamerList::DRIVING_INTERIOR;
		}

		if (*lists.GetDriveModePtr())
		{
			streamMode |= CSceneStreamerList::DRIVING_LOD;
		}
	}
	lists.SetStreamMode(streamMode);

	bool asyncWorkCompleted = false;
	while (true)
	{
		availableEntries = sysInterlockedRead( &m_ResultBlockCount );
		sysMemReadBarrier();
		if ( SCL_LOGGING_ONLY(g_forceWait ||) currentEntryIndex >= availableEntries )
		{
			extern u32 g_scanTasksPending;
#if __WIN32PC
			extern sysIpcEvent g_scanTasksReady;
#endif

			if ( SCL_LOGGING_ONLY(g_forceWait ||) sysInterlockedRead(&g_scanTasksPending) >  0 )
			{
#if SCL_LOGGING
				if (g_forceWait)
				{
					sysIpcSleep(u32(waitTime) + 1);
					continue;
				}
#endif
				PF_PUSH_TIMEBAR("Wait for more PVS Entries2");
				do 
				{
#if __WIN32PC
					WaitForSingleObject(g_scanTasksReady, 1);
#else
					sysIpcSleep(0);
#endif

					// Re-read the entries counter.
					availableEntries = sysInterlockedRead( &m_ResultBlockCount );
				} while ( sysInterlockedRead(&g_scanTasksPending) >  0 && currentEntryIndex >= availableEntries );
				PF_POP_TIMEBAR();

				continue;
			}
			else if (!asyncWorkCompleted)
			{
				// Must re-check m_ResultBlockCount after we see
				// g_scanTasksPending go to zero, otherwise there is a race
				// condition where we can miss calling pre-render on some
				// entities.
				asyncWorkCompleted = true;
				// Read barrier here to ensure that even with out-of-order
				// speculative execution, m_ResultBlockCount is read after
				// g_scanTasksPending.
				sysMemReadBarrier();
				continue;
			}
			else
			{
				// We are over the available entries count because 
				// there are no more jobs to process - exit the loop.
				break;
			}
		}

		ProcessResultBlock(currentEntryIndex++, viewportBits, lastVisibleFrame);
	}
	PF_POP_TIMEBAR();

	// Scan tasks aren't checking for mem overwrite, so this is fatal.
	FatalAssert(GetPVSEnd()-GetPVSBase() < MAX_PVS_SIZE);

#if __BANK
	TiledScreenCapture::SetCurrentPVSSize((int)(GetPVSEnd()-GetPVSBase()));
#endif // __BANK

	// Kick off the PVS sort job as soon as possible to give it the most time to finish.
	StartSortPVSJob();

	PF_PUSH_TIMEBAR("Gather Entities from Search and Always PreRender");
	m_nonVisiblePreRenderList.Clear();

	fwPtrNodeSingleLink* pNode = m_alwaysPreRenderList.GetHeadPtr();
	while (pNode)
	{
		CEntity* pEntity = (CEntity*) pNode->GetPtr();
		if (pEntity)
		{
			AddToNonVisiblePreRenderList(pEntity);
		}
		pNode = (fwPtrNodeSingleLink*) pNode->GetNextPtr();

	}

	if ( g_scanDebugFlags & SCAN_DEBUG_ENABLE_PRERENDER_SPHERE )
	{

		EndAsyncSearchForAdditionalPreRenderEntities();
		spdSphere nearCameraSphere(VECTOR3_TO_VEC3V(camInterface::GetPos()), ScalarV(20.0f));
		ms_PreRenderVehicleSearch.ExecuteCallbackOnResult(PreRenderSearchCBVehicle, &nearCameraSphere);
		ms_PreRenderPedAndObjectSearch.ExecuteCallbackOnResult(PreRenderSearchCBNonVehicle, NULL);
	}
	PF_POP_TIMEBAR();

	// Add peds that update animation but are not visible in viewport
	CPed::Pool* pedPool = CPed::GetPool();
	for(s32 i = 0; i < pedPool->GetSize(); i++)
	{
		CPed* pPed = pedPool->GetSlot(i);
		if(pPed && pPed->GetPedAiLod().ShouldUpdateAnimsThisFrame() && !pPed->GetIsVisibleInSomeViewportThisFrame() )
		{
			gPostScan.AddToNonVisiblePreRenderList(pPed);
		}
	}

	CleanAndSortPreRenderLists();

	PreRenderNonVisibleEntities();

	// We can only access the Scan results from the main thread, so cache the result
	// here. The BuildRenderListsFromPVS dependency job will need it.
	m_cameraInInterior = fwScan::GetScanResults().m_startingSceneNodeInInterior; 

#if __ASSERT
	PF_PUSH_TIMEBAR("Clear PreRendered flag");
	fwEntityPreRenderList& alreadyPreRenderedList = gRenderListGroup.GetAlreadyPreRenderedList();
	int entityCount = alreadyPreRenderedList.GetCount();
	for(int i = 0; i < entityCount; i++)
	{
		CEntity *pEntity = (CEntity*)alreadyPreRenderedList.GetEntity(i);
		if(pEntity)
			pEntity->SetPreRenderedThisFrame(false);
	}
	PF_POP_TIMEBAR();
#endif
}

void	CPostScan::ProcessPVSForRenderPhase()
{
	PF_AUTO_PUSH_TIMEBAR_BUDGETED("Process PVS For RP", 1.0f);

	ms_lateLights.ResetCount();
	ms_bAllowLateLightEntities = true;
	ms_LateLightsNearCameraSphere = spdSphere(VECTOR3_TO_VEC3V(camInterface::GetPos()), ScalarV(40.0f));

	//////////////////////////////////////////////////////////////////////////
	// process scene streaming based on PVS contents
	PF_PUSH_TIMEBAR("Scene streaming");
	{
		g_SceneStreamerMgr.Process();
		CStreamVolumeMgr::UpdateVolumesLoadedState();												// TODO: move this logic into the scene streamer update
																									// TODO: have scene streaming write back a "loaded" flag into the pvs results?
	}
	PF_POP_TIMEBAR();

	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	PF_PUSH_TIMEBAR("Finish PreRender");
	{
		FinishPreRender();
	}
	PF_POP_TIMEBAR();
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// update alpha values etc
	PF_PUSH_TIMEBAR("Process Post Visibility");
	{
		ProcessPostVisibility();
	}
	PF_POP_TIMEBAR();

#if __BANK
	PF_PUSH_TIMEBAR("Process Debug");
	CPostScanDebug::ProcessDebug2(GetPVSBase(), GetPVSEnd());													// TODO look at reducing
	PF_POP_TIMEBAR();
#endif // __BANK

	// Remove sort biases for deleted entities
	for(int i = ms_sortBiases.GetCount() - 1; i >=0; i--)
	{
		if(!ms_sortBiases[i].entity.Get())
		{
			ms_sortBiases.Delete(i);
		}
	}

	g_BuildRenderListsFromPVSDependencyRunning = 1;
	sysDependencyScheduler::Insert( &g_BuildRenderListsFromPVSDependency );	

	PF_PUSH_TIMEBAR_BUDGETED("Process Visible Lights", 0.4f);
	//
	// convert light entities into light sources for rendering
	//
	typedef atArray< std::pair< CEntity*, fwVisibilityFlags > >::iterator LightsIterator;
	for (LightsIterator it=m_lights.begin(); it!=m_lights.end(); ++it)
	{
		reinterpret_cast< CLightEntity* >( it->first )->ProcessVisibleLight( it->second );
	}

	fwVisibilityFlags forcedVisibleFlags;
	forcedVisibleFlags.Clear();
	forcedVisibleFlags.SetVisBit(g_SceneToGBufferPhaseNew->GetEntityListIndex());
	forcedVisibleFlags.SetVisBit(CMirrors::GetMirrorRenderPhase()->GetEntityListIndex());

	for(u32 i=0; i< ms_lateLights.GetCount(); i++)
	{
		reinterpret_cast< CLightEntity* >(ms_lateLights[i])->ProcessVisibleLight(forcedVisibleFlags);
	}

	PF_POP_TIMEBAR();

	PF_PUSH_TIMEBAR("Process LOD Light Visibility");
	CLODLights::ProcessVisibility();
	PF_POP_TIMEBAR();

	BANK_ONLY(if(DebugLighting::m_DistantLightsProcessVisibilityAsync))
	{
		PF_PUSH_TIMEBAR("Process Distant Lights Visibility");
		g_distantLights.ProcessVisibility();
		PF_POP_TIMEBAR();

	}
#if GTA_REPLAY
	//Not sure if this is the best place to call this but needs to be somewhere so the
	//drawlist gets setup correctly.
	ReplayLightingManager::AddLightsToScene();
#endif
}

static int GetAttachmentDepth(const fwEntity* entity)
{
	const fwEntity*		current = entity;
	int					depth = 0;

	while ( current->GetAttachParent() )
	{
		++depth;
		current = current->GetAttachParent();
	}

	return depth;
}

static bool AttachmentDepthCompare(const fwEntity* a, fwEntity* const b) {
	return GetAttachmentDepth(a) < GetAttachmentDepth(b);
}

static void SortByAttachmentDepth(fwEntityPreRenderList& list) {
	std::sort( list.begin(), list.end(), AttachmentDepthCompare );
}

void CPostScan::FinishPreRender()
{
	fwEntityPreRenderList& preRenderList = gRenderListGroup.GetPreRenderList();

	PF_PUSH_TIMEBAR("SortByAttachmentDepth");
	SortByAttachmentDepth( preRenderList );
	PF_POP_TIMEBAR();

	fwAnimDirectorComponentMotionTree::SetLockedGlobal(true, fwAnimDirectorComponent::kPhasePreRender);

	for (int i = 0; i < preRenderList.GetCount(); ++i)
	{
		CEntity* pEntity = static_cast< CEntity* >( preRenderList.GetEntity(i) );
		Assert( pEntity );

#if __BANK
		CPostScanDebug::ProcessPreRenderEntity( pEntity );
#endif
		// Visibility scanning is completely done by now, so GetIsVisibleInSomeViewportThisFrame() 
		// should return the correct visibility for the main viewport this frame - this method is 
		// only available on dynamic entities, however.
		const bool bIsEntityVisibleInMainViewport = pEntity->GetIsDynamic() ? static_cast<CDynamicEntity*>(pEntity)->GetIsVisibleInSomeViewportThisFrame() : true;
		if ( pEntity->PreRender(bIsEntityVisibleInMainViewport) != PRERENDER_DONE )
			gRenderListGroup.GetPreRender2List().AddEntity( pEntity );

		BANK_ONLY( CPostScanDebug::GetNumModelsPreRenderedRef() += 1; )
	}

	// Some DummyObjects may need to get their ambient scales updated this frame - this call
	// will queue them up for async processing
	CObjectPopulation::SubmitDummyObjectsForAmbientScaleCalc();
	
	CTimeCycleAsyncQueryMgr::StartAsyncProcessing();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ProcessPostVisibility
// PURPOSE:		runs over pvs and performs a variety of tasks including alpha
//				update, marking of important dummy objects for promotion, etc
//////////////////////////////////////////////////////////////////////////
void CPostScan::ProcessPostVisibility()
{
#if __BANK
	// don't update alphas if render is locked
	extern bool g_render_lock;
	if (g_render_lock)
		return;
#endif	//__BANK

	WaitForSortPVSDependency();
	CGtaPvsEntry* pStart = GetPVSBase();
	CGtaPvsEntry* pStop = GetPVSEnd();

#if __DEV
	if(CPostScanDebug::ShouldVerifyPvsSort())
	{
		PF_PUSH_TIMEBAR("Verify PVS Sort by LOD");
		CGtaPvsEntry* RESTRICT pPvsEntry = (CGtaPvsEntry* RESTRICT) pStart;
		while (pPvsEntry != pStop)
		{
			CGtaPvsEntry* pNextEntry = pPvsEntry + 1;
			if(pNextEntry != pStop)
			{
				// Assert that the PVS entries are sorted in ascending LOD sort value.
				Assertf(pPvsEntry->GetVisFlags().GetLodSortVal() <= pNextEntry->GetVisFlags().GetLodSortVal(), 
					"CPostScan::ProcessPostVisibility - PVS is not sorted in the correct LOD order!");
			}
			pPvsEntry = pNextEntry;
		}
		PF_POP_TIMEBAR();
	}
#endif

#if STREAMING_VISUALIZE
	if (strStreamingVisualize::IsInstantiated())
	{
		CGtaPvsEntry* RESTRICT pPvsEntry = (CGtaPvsEntry* RESTRICT) pStart;
		while (pPvsEntry != pStop)
		{
			CEntity* RESTRICT pEntity = (CEntity* RESTRICT) pPvsEntry->GetEntity();
			fwVisibilityFlags& visFlags = pPvsEntry->GetVisFlags();
			if (pEntity && CStreamingLists::IsRequired(pEntity) && ((visFlags.GetOptionFlags() & VIS_FLAG_ONLY_STREAM_ENTITY) == 0) && visFlags.GetAlpha() && !pEntity->GetDrawHandlerPtr() )
			{
				STRVIS_MARK_MISSING_LOD(pEntity, false);
			}

			++pPvsEntry;
		}
	}
#endif // STREAMING_VISUALIZE

#if __BANK
	PF_PUSH_TIMEBAR("Post Scan Debug 1");
	CPostScanDebug::ProcessDebug1(pStart, pStop);
	PF_POP_TIMEBAR(); 	// Post Scan Debug 1
#endif

	CVehicle::ProcessPostVisibility();

	PF_PUSH_TIMEBAR("Post Sort PVS Loop");

	//////////////////////////////////////////////////////////////////////////
	const bool	bSlodMode				= g_LodMgr.IsSlodModeActive();
	const bool	bAllowTimeBaseFading	= g_LodMgr.AllowTimeBasedFadingThisFrame();
	const u32	gbufPhaseFlag			= 1 << g_SceneToGBufferPhaseNew->GetEntityListIndex();
	const u32	mirrorPhaseFlag			= 1 << g_MirrorReflectionPhase->GetEntityListIndex();
	const bool	bDrivingFast			= g_ContinuityMgr.HighSpeedDriving();
	const bool	bSniping				= g_ContinuityMgr.IsUsingSniperOrTelescope();

#if !__FINAL
	const camBaseCamera* renderedDebugCamera = camInterface::GetDebugDirector().GetRenderedCamera();
	const bool bUsingDebugCam			= (renderedDebugCamera && camInterface::IsDominantRenderedCamera(*renderedDebugCamera));
#else // !__FINAL
	const bool bUsingDebugCam			= false;
#endif // !__FINAL
	const bool	bSrlPlayback			= gStreamingRequestList.IsPlaybackActive() && !bUsingDebugCam && gStreamingRequestList.IsLongJumpMode();
	const CViewport* pGameViewport		= gVpMan.GetGameViewport();

	m_lights.ResetCount();

#if __BANK
	const bool	bDoAlphaUpdate			= CPostScanDebug::DoAlphaUpdate();
	bool forceLodForCSS					= CPostScanDebug::ms_enableForceLodInCCS;
#endif	//__BANK

	const s32 gCCSIndex = g_CascadeShadowsNew->GetEntityListIndex();
	const s32 gCCSPhaseMask = 1 << gCCSIndex;

	bool bAllowSuppressingExternalLod = true;
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
	{
		const camBaseCamera* renderedCamera = camInterface::GetDominantRenderedCamera();
		if(renderedCamera && !renderedCamera->GetAttachParent())
		{
			CInteriorInst* pIntInst = CPortalVisTracker::GetPrimaryInteriorInst();
			s32 roomIdx = CPortalVisTracker::GetPrimaryRoomIdx();

			if (!pIntInst || roomIdx == INTLOC_INVALID_INDEX)
				bAllowSuppressingExternalLod = false;
		}
	}
#endif //GTA_REPLAY


	CGtaPvsEntry* RESTRICT pPvsEntry = (CGtaPvsEntry* RESTRICT) pStart;

	//////////////////////////////////////////////////////////////////////////
	// alpha update pass part #1 - assign fade-up values to everything and update parent LODs
	PF_PUSH_TIMEBAR("Alpha Part 1");
	BANK_ONLY( if (bDoAlphaUpdate) )
	{
		while (pPvsEntry != pStop)
		{
			CGtaPvsEntry* pNextEntry = pPvsEntry + 1;
			PrefetchDC((char*)pPvsEntry+128);
			if(Likely(pNextEntry < pStop))
			{
				PrefetchDC(pNextEntry->GetEntity());
			}

			CEntity* RESTRICT pEntity = (CEntity* RESTRICT) pPvsEntry->GetEntity();
			// Because the PVS partition/sort by lod now eliminates the "holes" (invalidated entries) 
			// in the PVS, we can simply assert on the entity instead of taking the branch (and possible 
			// penalty) to check it.
			Assert(pEntity);

			fwVisibilityFlags& visFlags = pPvsEntry->GetVisFlags();
			if(visFlags.GetOptionFlags() & VIS_FLAG_ONLY_STREAM_ENTITY)
			{
				pPvsEntry = pNextEntry;
				continue;
			}

			fwLodData& lodData = pEntity->GetLodData();
			const bool bVisibleInGbuf = ((visFlags & gbufPhaseFlag) != 0);
			const bool bVisibleInMirrors = ((visFlags & mirrorPhaseFlag) != 0);

			// Performance optimization - Use existence of draw handler as a proxy for whether or not the model info 
			// has loaded. Otherwise we cache miss on the model info, which is very expensive to do for each entity 
			// in this loop.
			const bool bIsLoaded = pEntity->GetDrawHandlerPtr() ? true : pEntity->GetBaseModelInfo()->GetHasLoaded();
			const bool bResetAlpha = lodData.GetResetAlpha();
			const bool bIsResetDisabled = lodData.IsResetDisabled();

			Assert(bIsLoaded == pEntity->GetBaseModelInfo()->GetHasLoaded());

			// Common case is that we're not in SLod mode - check for that to avoid branch mis-prediction.
			if (!bSlodMode)
			{
				//////////////////////////////////////////////////////////////////////////
				// A: reset child alpha (as list is sorted by lod level)
				pEntity->ResetChildAlpha();

				// A2 - re-examine to see how much of this is really necessary
				if (bAllowTimeBaseFading && bVisibleInGbuf && bResetAlpha && !bIsResetDisabled)
				{
					pEntity->SetAlpha(LODTYPES_ALPHA_INVISIBLE); // entity has had drawable created, so force alpha down to 0
				}

				// Moved the flag setting here to avoid a LHS
				lodData.SetVisible( bVisibleInGbuf || bVisibleInMirrors);
				lodData.SetLoaded( bIsLoaded );
				// B: calculate new alpha value
				lodData.SetResetAlpha(false);	// clear reset flag
				lodData.SetChildDrawing(false);	// water reflection code needs to know if something has been faded down relative to its children :/
				u32 alpha = g_LodMgr.GetAlphaPt1_FadeUp( lodData, visFlags.GetAlpha() );
				lodData.SetAlpha(alpha);
				lodData.SetForceOnInCascadeShadows(false);

				// C: update parent's child alpha value
				CEntity* pLod = (CEntity*) pEntity->GetLod();
				if (pLod)
				{
					pLod->UpdateChildAlpha( alpha );
				}
				//////////////////////////////////////////////////////////////////////////
			}
			else
			{
				// Moved the flag setting here to avoid a LHS in the other section of this if statement
				lodData.SetVisible( bVisibleInGbuf );
				lodData.SetLoaded( bIsLoaded );
				g_LodMgr.UpdateAlpha_SlodMode( lodData );
			}
			pPvsEntry++;
		}
	}
	PF_POP_TIMEBAR(); // "Alpha Part 1"

	//////////////////////////////////////////////////////////////////////////
	PF_PUSH_TIMEBAR("Alpha Part 2 + Misc");
	const bool bPerformAlphaUpdateSecondPart = (!bSlodMode BANK_ONLY(&& bDoAlphaUpdate));

	pPvsEntry = (CGtaPvsEntry* RESTRICT) pStart;
	while (pPvsEntry != pStop)
	{
		CGtaPvsEntry* pNextEntry = pPvsEntry + 1;
		PrefetchDC((char*)pPvsEntry+128);
		if(Likely(pNextEntry < pStop))
		{
			PrefetchDC(pNextEntry->GetEntity());
		}

		CEntity* RESTRICT pEntity = (CEntity* RESTRICT) pPvsEntry->GetEntity();
		// See comment in above loop - holes have been eliminated, and previous loop doesn't 
		// create any additional PVS holes by invalidating entries, so simply assert on the entity.
		Assert(pEntity);

		fwVisibilityFlags& visFlags = pPvsEntry->GetVisFlags();
		if(visFlags.GetOptionFlags() & VIS_FLAG_ONLY_STREAM_ENTITY)
		{
			pEntity->SetAlpha( visFlags.GetAlpha() );
			pPvsEntry->Invalidate();

			pPvsEntry = pNextEntry;
			continue;
		}

		fwLodData& lodData = pEntity->GetLodData();
		bool bisLoaded = lodData.IsLoaded();

		//////////////////////////////////////////////////////////////////////////
		// alpha update pass #2
		if ( Likely(bPerformAlphaUpdateSecondPart) )
		{
			// Prefetch the entity's transform, as it will be needed later when we call UpdateAlphaPt2_FadeDownRelativeToChildren().
			PrefetchDC(pEntity->GetTransformPtr());

			CEntity* pLod = NULL;
			const u32 numChildren = lodData.GetNumChildren();
			const bool allChildrenAttached = lodData.AllChildrenAttached();
			const bool bMayForceUpGrandparent =  (bSniping || bDrivingFast) && lodData.IsLoddedHd();

			const bool bUseRelatedDummyLod = (visFlags.GetOptionFlags()&VIS_FLAG_ENTITY_NEVER_DUMMY)!=0 && 
											visFlags.GetEntityType()==ENTITY_TYPE_OBJECT && 
											static_cast<CObject*>(pEntity)->GetRelatedDummy();
			// Common case is not an object with related dummy - check for that to avoid branch mis-prediction.
			if ( !bUseRelatedDummyLod )
			{
				pLod = (CEntity*) pEntity->GetLod();
			}
			else
			{
				// this is disgusting, but special behaviour for a type of object only used a couple of times in the entire map.
				// that is - windmills, pumpjacks, ferris wheel etc. normally dynamic objects don't have lods, but in these
				// cases (animated objects, fragments) we can't render the dummy object ever (because it cannot animate)
				pLod = (CEntity*) static_cast<CObject*>(pEntity)->GetRelatedDummy()->GetLod();
			}

			//////////////////////////////////////////////////////////////////////////
			// A: pick up alpha value from parent's child alpha
			if ( pLod && pLod->GetChildAlpha() && visFlags.GetAlpha()!=LODTYPES_ALPHA_INVISIBLE)
			{
				pEntity->SetAlpha( pLod->GetChildAlpha() );
			}

			// B: fade down relative to children
			bool bShouldBeFullyVisible = ( visFlags.GetAlpha()==LODTYPES_ALPHA_VISIBLE );
			if ( numChildren && (bSrlPlayback || allChildrenAttached) )	// SRLs don't do full IMAP load, to avoid building pool problems etc
			{
				// Don't calculate this stuff if above isn't true.
				const bool bAvoidFadeRelatedToChildren = (visFlags.GetOptionFlags() & VIS_FLAG_RENDERED_THROUGH_LODS_ONLY_PORTAL) && lodData.IsLod();
				if(!bAvoidFadeRelatedToChildren)
				{
					g_LodMgr.UpdateAlphaPt2_FadeDownRelativeToChildren(pEntity, bShouldBeFullyVisible);
				}
			}

			// C: force LOD to draw if required
			if ( lodData.IsVisible() && pLod && lodData.GetAlpha() && !lodData.IsParentOfInterior() ) 
			{
				// Don't calculate this stuff if above isn't true.
				const bool bForceUpLodBecauseNotLoaded = ( !bisLoaded );
				const bool bForceUpLodBecauseFading = ( bShouldBeFullyVisible && lodData.IsFading() && g_LodMgr.AllowedToForceDrawParent(lodData) );
				if(bForceUpLodBecauseFading || bForceUpLodBecauseNotLoaded)
				{
					pLod->SetAlpha( LODTYPES_ALPHA_VISIBLE );

					//////////////////////////////////////////////////////////////////////////
					// for HD models only - if driving fast and forcing up the LOD has
					// failed (ie it hasn't streamed in yet) then we allow forcing up of SLOD1
					if (bMayForceUpGrandparent && bForceUpLodBecauseNotLoaded && !pLod->GetLodData().IsLoaded() && pLod->GetLodData().HasLod())
					{
						pLod->GetLod()->SetAlpha( LODTYPES_ALPHA_VISIBLE );
					}
					//////////////////////////////////////////////////////////////////////////
				}
			}

			if (BANK_ONLY(forceLodForCSS &&) ( lodData.IsLoddedHd() || lodData.IsLod() ) && !bisLoaded  && pLod && lodData.GetAlpha() && !lodData.IsVisible() && (visFlags.GetPhaseVisFlags() & gCCSPhaseMask))
			{
				pLod->GetLodData().SetForceOnInCascadeShadows(true);
			}

			// re-examine to see how much of this is really necessary
			if ( bisLoaded )
			{
				lodData.SetResetDisabled(false);
			}
// 				else
// 				{
// 					lodData.SetAlpha(LODTYPES_ALPHA_INVISIBLE);
// 				}

			// if interior shell not ready, then force draw the LOD if available
			if ( visFlags.GetEntityType() == ENTITY_TYPE_MLO )
			{
				fwLodData& lodData = pEntity->GetLodData();
				const u32 childDepth = lodData.GetChildLodType();

				CInteriorInst* pIntInst = static_cast<CInteriorInst*>(pEntity);
				if (pPvsEntry->GetDist() < ((float(pIntInst->GetLodDistance()) * g_LodScale.GetPerLodLevelScale(childDepth)) + LODTYPES_FADE_DIST + 5.0f))		// add fading distance and a bit
				{
					if (pLod)
					{
						if (pIntInst->SetAlphaForRoomZeroModels(pIntInst->GetAlpha()))
						{
							pLod->SetAlpha(LODTYPES_ALPHA_INVISIBLE);
						} else {
							pLod->SetAlpha(LODTYPES_ALPHA_VISIBLE);
						}
					}
					pIntInst->CapAlphaForContentsEntities();
				}
			}

			//////////////////////////////////////////////////////////////////////////
			// never render dummy objects which are set to never dummy
			if (visFlags.GetEntityType()==ENTITY_TYPE_DUMMY_OBJECT && pLod && (visFlags.GetOptionFlags()&VIS_FLAG_ENTITY_NEVER_DUMMY)!=0)
			{
				// special case - we never want to see these dummy objects!
				pEntity->SetAlpha( LODTYPES_ALPHA_INVISIBLE );
				pLod->SetAlpha( LODTYPES_ALPHA_VISIBLE );
			}

		}	/* end of alpha update pt 2 */
		//////////////////////////////////////////////////////////////////////////

		//////////////////////////////////////////////////////////////////////////
		// misc + extract important objects / dummy objects
		switch ( visFlags.GetEntityType() )
		{
		case ENTITY_TYPE_DUMMY_OBJECT:
			if ( pPvsEntry->GetDist() < OBJECT_POPULATION_RESET_RANGE )
			{
				CObjectPopulation::InsertSortedDummyClose(pEntity, pPvsEntry->GetDist());
				pEntity->m_nFlags.bFoundInPvs = true;
			}
			break;
		case ENTITY_TYPE_OBJECT:
			if ( pPvsEntry->GetDist() < OBJECT_POPULATION_RESET_RANGE )
			{
				pEntity->m_nFlags.bFoundInPvs = true;
			}
			break;
		case ENTITY_TYPE_PORTAL:
			//
			// update scanned portals
			//
			{
				if ( pGameViewport )
				{
					CRenderer::UpdateScannedVisiblePortal( reinterpret_cast<CPortalInst*>(pEntity) );
				}
			}
			break;

		case ENTITY_TYPE_LIGHT:
			//
			// push light entities into a separate list for conversion into light sources for rendering
			//
			{
				//let's not add the light source to the list if it's not visible in the main viewport and its beyond a certain cutoff distance
				if(lodData.IsVisible() || pPvsEntry->GetDist() < Lights::GetNonGBufLightCutoffDistance())
				{
					m_lights.Grow() = std::make_pair( pEntity, visFlags );
				}
				pPvsEntry->Invalidate();
			}
			break;

		case ENTITY_TYPE_ANIMATED_BUILDING:
			//
			// force large nearby animated buildings to animate even if off screen
			//
			{
				if ( !lodData.IsVisible()
					&& (visFlags.GetOptionFlags() & VIS_FLAG_ONLY_STREAM_ENTITY)==0 
					&& pPvsEntry->GetDist()<=CAnimatedBuilding::FORCEANIM_DIST_MAX
					&& pEntity->GetBaseModelInfo()->GetBoundingSphereRadius()>=CAnimatedBuilding::FORCEANIM_SIZE_MIN )
				{
					reinterpret_cast< CDynamicEntity* >( pEntity )->m_nDEflags.bForcePrePhysicsAnimUpdate = true;
				}
			}
			break;

		default:
			break;
		}

		// make sure no one tries to render an entity which is not loaded.
		if (!bisLoaded)
		{
			pPvsEntry->Invalidate();
		}

		//////////////////////////////////////////////////////////////////////////

		pPvsEntry = pNextEntry;
	}
	PF_POP_TIMEBAR(); // "Alpha Part 2 + Misc"

	PF_POP_TIMEBAR(); // "Post Sort PVS Loop"

	// suppress the interior LOD of the MLO the player is actually in
	CInteriorInst* pPlayerIntInst = NULL;
	CPed* pPlayer = CPedFactory::GetFactory()->GetLocalPlayer();
	if (pPlayer && pPlayer->GetIsInInterior() && bAllowSuppressingExternalLod)
	{
		CInteriorInst* pPlayerIntInst = pPlayer->GetPortalTracker()->GetInteriorInst();

		if (pPlayerIntInst)
		{
			if (pPlayerIntInst->SetAlphaForRoomZeroModels(LODTYPES_ALPHA_VISIBLE))
			{
				pPlayerIntInst->SuppressInternalLOD();
			}

			if (!pPlayerIntInst->GetGroupId())
			{
				pPlayerIntInst->SuppressExternalLOD();
			}
		}
	}

	CInteriorInst* pPrimaryIntInst = CPortalVisTracker::GetPrimaryInteriorInst();
	if (pPrimaryIntInst)
	{
		if ((pPrimaryIntInst != pPlayerIntInst))
		{
			if (pPrimaryIntInst->SetAlphaForRoomZeroModels(LODTYPES_ALPHA_VISIBLE))
			{
				pPrimaryIntInst->SuppressInternalLOD();
			}
		}

		// If the exterior is not visible in the GBuffer
		if (!fwScan::GetScanResults().GetIsExteriorVisibleInGbuf())
		{
			// Look through any linked portals attached to the room and suppress their LODs
			// Note we do this for 2 link interiors 
			spdTransposedPlaneSet8 frustum;
			frustum.Set(g_SceneToGBufferPhaseNew->GetViewport()->GetGrcViewport());

 			const int maxVisitedLocations = 0;
			atArray<fwInteriorLocation> visitedLocs(0, maxVisitedLocations);
			ForceUpLinkedInteriors(CPortalVisTracker::GetInteriorLocation(), pPrimaryIntInst, frustum, visitedLocs);
		}
	}

	ms_bAllowLateLightEntities = false;

	// Apply scripted alpha override 
	if( ms_hasAnyAlphaOverride )
	{
		int lastAlphaOverride = 0;
		bool hasAnyAlphaOverride = false;
		for(int i=0;i<ms_lastAlphaOverride;i++)
		{
			alphaOverride& alphaO = ms_alphaOverrides[i];
		
			if( alphaO.entity )
			{
				lastAlphaOverride = i;
				hasAnyAlphaOverride = true;
				alphaO.entity->SetAlpha((u8)alphaO.alpha&0xff);
			}
		}

		ms_hasAnyAlphaOverride = hasAnyAlphaOverride;
		ms_lastAlphaOverride = lastAlphaOverride + 1;
	}


	// appalling multiplayer apartment hack - cull exterior HD stuff inside the specified sphere from gbuffer
	// this was done because we have these multiplayer apartments you can buy, but they don't properly fit inside the buildings
	// they are located in. so script hide some of the models. burn this code upon reading
	g_PostScanCull.Process(pStart, pStop);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	BuildRenderListsFromPVS
// PURPOSE: process the PVS from start to finish and place each entry in the appropriate renderlists.
//			Also adds any entries to the pre-render list which need to be
//////////////////////////////////////////////////////////////////////////
void CPostScan::BuildRenderListsFromPVS()
{
	CGtaPvsEntry* pStart		= GetPVSBase();
	CGtaPvsEntry* pStop			= GetPVSEnd();
	u32 canRenderAlpha	= CRenderer::GetCanRenderAlphaFlags();
	u32 canRenderWater	= CRenderer::GetCanRenderWaterFlags();
	u32 canRenderCutout	= CRenderer::GetCanRenderScreenDoorFlags();
	u32 canRenderDecal	= CRenderer::GetCanRenderDecalFlags();

#if __BANK
	if (Unlikely(CPostScanDebug::GetOverrideGBufBitsEnabled()))
	{
		canRenderWater = 0;
		canRenderDecal = 0;
	}
#endif // __BANK

	// Process sort biases set by script
	if(ms_sortBiases.GetCount())
	{
		int numBiases = ms_sortBiases.GetCount();
		CGtaPvsEntry* RESTRICT pPvsEntry = (CGtaPvsEntry* RESTRICT ) pStart;
		while(pPvsEntry != pStop)
		{
			CEntity* RESTRICT pEntity = (CEntity* RESTRICT) pPvsEntry->GetEntity();
			if(pEntity)
			{
				for(int i = 0; i < numBiases; i++)
				{
					if(ms_sortBiases[i].entity == pEntity)
					{
						pPvsEntry->AddToDist(ms_sortBiases[i].bias);
						break;
					}
				}
			}

			pPvsEntry++;
		}
	}

	CompileTimeAssert(VIS_FLAG_OPTION_INTERIOR == BIT(0)); // This function relies on VIS_FLAG_OPTION_INTERIOR being bit 0, if it has moved the code below will need to be updated
	u32 cameraInInterior = m_cameraInInterior;
	const float lodTypesMaxLodDist = (float)LODTYPES_MAX_LODDIST;

	s32 gBuffIndex = g_SceneToGBufferPhaseNew->GetEntityListIndex();
	s32 gBuffPhaseMask = 1 << gBuffIndex;
	u32 gbuffCameraInInterior = (cameraInInterior & gBuffPhaseMask) >> gBuffIndex;

	s32 reflectionPhaseIndex = g_ReflectionMapPhase->GetEntityListIndex();
	s32 reflectionPhaseMask = 1 << reflectionPhaseIndex;
	u32 reflectionCameraInInterior = (cameraInInterior & reflectionPhaseMask) >> reflectionPhaseIndex;

	s32 gCCSIndex = g_CascadeShadowsNew->GetEntityListIndex();

	atFixedArray<int, fwVisibilityFlags::MAX_PHASE_VISIBILITY_BITS> renderPhaseMask;
	atFixedArray<int, fwVisibilityFlags::MAX_PHASE_VISIBILITY_BITS> renderPhaseListIndex;
	for(u32 i=0; i<fwVisibilityFlags::MAX_PHASE_VISIBILITY_BITS; i++)
	{
		fwRenderListDesc& rlDesc	= gRenderListGroup.GetRenderListForPhase(i);
		CRenderPhase *renderPhase	= (CRenderPhase *) rlDesc.GetRenderPhaseNew();

		// the streaming cull shapes aren't backed by a real render phase
		if ( !renderPhase )
		{
			continue;
		}

		renderPhaseListIndex.Push(i);
		renderPhaseMask.Push(1<<i);
	}

	u32 phaseCount = renderPhaseListIndex.GetCount();

	u32 flagMask = fwEntity::HAS_ALPHA | fwEntity::HAS_DISPL_ALPHA | fwEntity::HAS_WATER | fwEntity::HAS_DECAL | fwEntity::HAS_CUTOUT;

#if __PS3
#define NUM_ENTITIES_TO_PROCESS_BEFORE_YIELDING		(50)
	int yieldCounter = 0;
#endif

#if __CHECK_FOR_DUPLICATES
	rage::atMap<CEntity*, CGtaPvsEntry*> duplicateMap;
#endif

#if !__FINAL
	m_EntityCount = 0;
#endif	//!__FINAL

	CGtaPvsEntry* RESTRICT pPvsEntry = (CGtaPvsEntry* RESTRICT ) pStart;
	while(pPvsEntry != pStop)
	{

#if __PS3
		if(yieldCounter == NUM_ENTITIES_TO_PROCESS_BEFORE_YIELDING)
		{
			yieldCounter = 0;
			sysIpcYield(0);
		}
#endif

		CGtaPvsEntry* pNextEntry = pPvsEntry + 1;
		PrefetchDC((char*)pPvsEntry + 128);
		if(Likely(pNextEntry < pStop))
		{
			PrefetchDC(pNextEntry->GetEntity());
		}

		CEntity* RESTRICT pEntity = (CEntity* RESTRICT) pPvsEntry->GetEntity();

		if(pEntity)
		{
#if __PS3
			yieldCounter++;
#endif

#if !__FINAL
			m_EntityCount++;
#endif	//!__FINAL

 #if __BANK 
 			// trace selected entity
			if (CPostScanDebug::ms_breakOnSelectedEntityInBuildRenderListFromPVS && pEntity == g_PickerManager.GetSelectedEntity())
			{
				__debugbreak();
			}
 #endif // __BANK

			//////////////////////////////////////////////////////////////////////////
			// required info:
			// 1. is alpha less than threshold value	(from entity / loddata)
			// 2. is alpha less than 255				(from entity / loddata)
			// 3. is it a tree							(from entity / loddata)
			// 4. base flags							(from entity)
			// 5. vis flags								(from pvs entry)
			// 6. entity ptr							(from pvs entry)
			// 7. distance								(from pvs entry)
			//////////////////////////////////////////////////////////////////////////

			fwVisibilityFlags& visFlags		= pPvsEntry->GetVisFlags();

			Assertf( (visFlags.GetOptionFlags() & VIS_FLAG_ONLY_STREAM_ENTITY)==0, "%s is flagged as streaming only, so shouldn't be getting through to build render lists", pEntity->GetModelName() );

			u32 optionFlags					= visFlags.GetOptionFlags();
			const float fDistanceToCamera	= pPvsEntry->GetDist();
			const u32 perPhaseRenderFlags	= visFlags.GetPhaseVisFlags();
			const u32 subphaseVisFlags		= visFlags.GetSubphaseVisFlags();
			const bool pedInVehicle			= (optionFlags & VIS_FLAG_ENTITY_IS_PED_INSIDE_VEHICLE) != 0;
			const bool hdTexCapable			= (optionFlags & VIS_FLAG_ENTITY_IS_HD_TEX_CAPABLE) != 0;

			u16 baseCustomFlags				= pedInVehicle ? (fwEntityList::CUSTOM_FLAGS_PED_IN_VEHICLE) : 0;
			baseCustomFlags				   |= hdTexCapable ? (fwEntityList::CUSTOM_FLAGS_HD_TEX_CAPABLE) : 0;

			u16 inInterior					= optionFlags & VIS_FLAG_OPTION_INTERIOR;

			// Flipping the lodSortValue here so it works with an ascending sort 
			const u16 hdLodSortValue = (u16)fwLodData::GetSortVal(LODTYPES_DEPTH_HD);
			u16 flippedCustomLodSortValue = hdLodSortValue - (u16)visFlags.GetLodSortVal();
			Assertf(flippedCustomLodSortValue <= hdLodSortValue, "visFlags.GetLodSortVal() returned a bad value %d", visFlags.GetLodSortVal());

			const bool bIsPed = pEntity->GetIsTypePed();
			const bool bIsVehicle = pEntity->GetIsTypeVehicle();
			// We have additional special checks for peds & vehicles that may cause them to be excluded from these passes
			bool bShouldDoAlpha = true;
			bool bShouldDoDecal = true;
			bool bShouldDoCutout = true;
			if(bIsPed)
			{
				CPed* pPed = pPed = static_cast<CPed*>(pEntity);
				u8 pedLodIndex = pedLodIndex = pPed->GetModelLodIndex();
				if((pedLodIndex == 3) && !pPed->GetLodBucketInfoExists(pedLodIndex) && pPed->GetLodBucketInfoExists(pedLodIndex-1))
				{
					// If the ped is supposed to be in SLOD mode (lod == 3), and the info 
					// doesn't exist for SLOD, but does exist for 2, this is because we sometimes 
					// don't have a SLOD model, and will simply be rendered at lod = 2, use that index instead.
					pedLodIndex--;
				}

				Assert(pPed->GetDrawHandlerPtr());
				const CPedPropData& pedPropData = pPed->GetPedDrawHandler().GetPropData();

				// 1 - A Ped with a prop that has Alpha/Decal/Cutout, 
				//		------ OR ---
				// 2 - A Ped and LOD bucket mask info does not exist.
				// 3 - A Ped and LOD bucket mask info does exist, and it says the ped has alpha/decal/cutout at the specified LOD level.
				bShouldDoAlpha = pedPropData.GetIsCurrPropsAlpha() || !pPed->GetLodBucketInfoExists(pedLodIndex) || pPed->GetLodHasAlpha(pedLodIndex);
				bShouldDoDecal = pedPropData.GetIsCurrPropsDecal() || !pPed->GetLodBucketInfoExists(pedLodIndex) || pPed->GetLodHasDecal(pedLodIndex);
				bShouldDoCutout = pedPropData.GetIsCurrPropsCutout() || !pPed->GetLodBucketInfoExists(pedLodIndex) || pPed->GetLodHasCutout(pedLodIndex);
			}
			else if(bIsVehicle)
			{
				CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);
				CVehicleModelInfo *pVMI = pVehicle->GetVehicleModelInfo();
				bShouldDoAlpha = !pVehicle->GetIsLandVehicle() || (fDistanceToCamera < pVMI->GetVehicleLodDistance(VLT_LOD2));
			}

#if __CHECK_FOR_DUPLICATES
			if (pEntity->GetIsDynamic() && static_cast<CDynamicEntity*>(pEntity)->IsStraddlingPortal())
			{
				CGtaPvsEntry **pDup = duplicateMap.Access(pEntity);
				if (pDup)
				{
					Assertf((*pDup)->GetEntity() != pEntity, "Duplicate Entries in PVS");					
				}
				else
				{
					duplicateMap.Insert(pEntity, pPvsEntry);
				}
			}
#endif
			// for each phase, determine if entity should be added to associated renderList and if so, tagged for which pass
			for(u32 i=0; i<phaseCount; i++)
			{
				if (perPhaseRenderFlags & renderPhaseMask[i])
				{
					u16 customFlags					= baseCustomFlags;
					u16 alphaCustomFlags			= baseCustomFlags;
					float customDistanceToCamera	= fDistanceToCamera;

					fwRenderListDesc& rlDesc		= gRenderListGroup.GetRenderListForPhase(renderPhaseListIndex[i]);
					CRenderPhase *renderPhase		= (CRenderPhase *) rlDesc.GetRenderPhaseNew();
					bool bRenderingShadows			= CDrawListMgr::IsShadowDrawList(renderPhase->GetDrawListType());
					bool bParabReflectionPhase		= (renderPhase->GetDrawListType() == DL_RENDERPHASE_REFLECTION_MAP);

					if (renderPhase->GetDrawListType() == DL_RENDERPHASE_GBUF)
					{
#if __BANK
						if (CPostScanDebug::ms_enableSortingAndExteriorScreenQuadScissoring)
#endif
						{
							// interiorSortGBuff is 1 if 'the entity and the camera are in an interior' or 'the entity and the camera are in the in exterior'
							u16 interiorSortGBuff = u16(~(inInterior ^ gbuffCameraInInterior)) & 0x1;

							u32 entityType = visFlags.GetEntityType();
							const bool dontScissorDynamicEntities = BANK_SWITCH(CPostScanDebug::ms_dontScissorDynamicEntities, true);
							u16 notDynamicEntity = dontScissorDynamicEntities ? u16(entityType < ENTITY_TYPE_ANIMATED_BUILDING || entityType > ENTITY_TYPE_OBJECT) : 1;

							customFlags |= interiorSortGBuff << fwEntityList::CUSTOM_FLAGS_INTERIOR_SORT_BIT;
							customFlags |=  ((~inInterior & gbuffCameraInInterior) & notDynamicEntity) << fwEntityList::CUSTOM_FLAGS_USE_EXTERIOR_SCREEN_QUAD_BIT;
						}
					}
					else if (bParabReflectionPhase)
					{
						// interiorSortReflection is 1 if 'the entity and the camera are in an interior' or 'the entity and the camera are in the in exterior'
						u16 interiorSortReflection = u16(~(inInterior ^ reflectionCameraInInterior)) & 0x1;
						customFlags |= interiorSortReflection << fwEntityList::CUSTOM_FLAGS_INTERIOR_SORT_BIT;
					}
					else if (renderPhase->GetDrawListType() == DL_RENDERPHASE_MIRROR_REFLECTION)
					{
						u16 interiorSortReflection = u16(inInterior) & 0x1;
						customFlags |= interiorSortReflection << fwEntityList::CUSTOM_FLAGS_INTERIOR_SORT_BIT;
						alphaCustomFlags = customFlags; // mirror reflection needs custom flags in alpha passes too
					}
#if WATER_REFLECTION_PRE_REFLECTED
					else if (renderPhase->GetDrawListType() == DL_RENDERPHASE_WATER_REFLECTION)
					{
						if (Unlikely(pEntity->GetRenderPhaseVisibilityMask().IsFlagSet(VIS_PHASE_MASK_WATER_REFLECTION_PROXY_PRE_REFLECTED)))
						{
							customFlags |= fwEntityList::CUSTOM_FLAGS_WATER_REFLECTION_PRE_REFLECTED;
						}

						customFlags |= fwEntityList::CUSTOM_FLAGS_WATER_REFLECTION; // this forces the FlagsChanged function to get called before anything else
						alphaCustomFlags = customFlags; // water reflection needs custom flags in alpha passes too
					}
#endif // WATER_REFLECTION_PRE_REFLECTED

 					// See if a lod mask is set
					if (renderPhase->GetLodMask() != fwRenderPhase::LODMASK_NONE) 
 					{
						if (bParabReflectionPhase)
						{
							customFlags |= (fwEntityList::CUSTOM_FLAGS_LOD_SORT_ENABLED | (flippedCustomLodSortValue << fwEntityList::CUSTOM_FLAGS_LOD_SORT_VAL_BIT0));
#if __ASSERT
							u32 invalidBits = ~u32( fwEntityList::CUSTOM_FLAGS_INTERIOR_SORT	  | 
													fwEntityList::CUSTOM_FLAGS_LOD_SORT_ENABLED   | 
													fwEntityList::CUSTOM_FLAGS_LOD_SORT_VAL_MASK  | 
													fwEntityList::CUSTOM_FLAGS_PED_IN_VEHICLE     |
													fwEntityList::CUSTOM_FLAGS_HD_TEX_CAPABLE );
							Assertf((customFlags & invalidBits) == 0, "Invalid bits set 0x%x", customFlags);
#endif
							const u32 SLod3SortValue = fwLodData::GetSortVal(LODTYPES_DEPTH_SLOD3);
							if (visFlags.GetLodSortVal() == SLod3SortValue)
							{
								customDistanceToCamera = lodTypesMaxLodDist - customDistanceToCamera;
							}
						}
 					}

					//////////////////////////////////////////////////////////////////////////
					// cull entities with low alpha values
					//
					bool bWaterReflectionPhase = (renderPhase->GetDrawListType() == DL_RENDERPHASE_WATER_REFLECTION);
					bool bForceCascadeShadowPhase = renderPhase->GetEntityListIndex() == gCCSIndex && pEntity->GetLodData().GetForceOnInCascadeShadows();
#if __BANK
					if (Unlikely(CPostScanDebug::GetOverrideGBufBitsEnabled()))
					{
						if (CPostScanDebug::GetOverrideGBufVisibilityType() == VIS_PHASE_WATER_REFLECTION)
						{
							if (renderPhase->GetDrawListType() == DL_RENDERPHASE_GBUF ||
								renderPhase->GetDrawListType() == DL_RENDERPHASE_DRAWSCENE ||
								renderPhase->GetDrawListType() == DL_RENDERPHASE_DEBUG ||
								renderPhase->GetDrawListType() == DL_RENDERPHASE_DEBUG_OVERLAY)
							{
								bWaterReflectionPhase = true; // use water reflection alpha
							}
						}
					}
#endif // __BANK
					u8 alpha = pEntity->GetAlpha();

					if (bWaterReflectionPhase && CRenderPhaseWaterReflectionInterface::ShouldEntityForceUpAlpha(pEntity))
					{
						alpha = LODTYPES_ALPHA_VISIBLE;
					}
					else if (alpha < ALPHA_CULL_THRESHOLD)
					{
						if (!bParabReflectionPhase && !bForceCascadeShadowPhase
							BANK_ONLY(&& !(bWaterReflectionPhase && !CRenderPhaseWaterReflectionInterface::CanAlphaCullInPostScan())))
						{
							continue;
						}
					}
					//////////////////////////////////////////////////////////////////////////

					// if tree type object then add to tree pass and skip to next object (don't process in any other passes)
 					if (pEntity->GetLodData().IsTree())
 					{
						rlDesc.AddEntity(pEntity, subphaseVisFlags, customDistanceToCamera, RPASS_TREE, customFlags);
						pEntity->SetAddedToDrawListThisFrame(renderPhase->GetDrawListType());
						continue;
 					}

					// if fading then add to fading pass and skip to next object (don't process in any other passes)
					if (!bParabReflectionPhase && alpha<LODTYPES_ALPHA_VISIBLE && !bForceCascadeShadowPhase)
					{
						rlDesc.AddEntity(pEntity, subphaseVisFlags, customDistanceToCamera, RPASS_FADING, customFlags);
						pEntity->SetAddedToDrawListThisFrame(renderPhase->GetDrawListType());
						continue;
					}

					// determine if object is placed in either the LOD pass or the VISIBLE pass
					if(pEntity->IsBaseFlagSet(fwEntity::HAS_OPAQUE))
					{
						if (pEntity->GetLodData().HasChildren() && !bParabReflectionPhase)
						{
							rlDesc.AddEntity(pEntity, subphaseVisFlags, customDistanceToCamera, RPASS_LOD, customFlags);
							pEntity->SetAddedToDrawListThisFrame(renderPhase->GetDrawListType());
						}
						else
						{
							if( !bRenderingShadows && pEntity->IsBaseFlagSet(fwEntity::FORCE_ALPHA) )
							{
								rlDesc.AddEntity(pEntity, subphaseVisFlags, customDistanceToCamera, RPASS_FADING, customFlags);
								pEntity->SetAddedToDrawListThisFrame(renderPhase->GetDrawListType());
							}
							else
							{
								rlDesc.AddEntity(pEntity, subphaseVisFlags, customDistanceToCamera, RPASS_VISIBLE, customFlags);
								pEntity->SetAddedToDrawListThisFrame(renderPhase->GetDrawListType());
							}
						}
					}

					if(pEntity->GetBaseFlags() & flagMask)
					{
						u32 phaseMask = renderPhaseMask[i];

						// add object to other lists if the object is flagged as containing any of the appropriate type of rendering material
						if (pEntity->IsBaseFlagSet(fwEntity::HAS_ALPHA) && (phaseMask&canRenderAlpha) && bShouldDoAlpha)
						{
							if (bIsPed)
							{
								rlDesc.AddEntity(pEntity, pEntity->GetBaseFlags() | fwEntity::DRAW_FIRST_SORTED, pEntity->GetRenderFlags(), subphaseVisFlags, customDistanceToCamera, RPASS_ALPHA, baseCustomFlags);
								pEntity->SetAddedToDrawListThisFrame(renderPhase->GetDrawListType());
							}
							else
							{
								rlDesc.AddEntity(pEntity, subphaseVisFlags, customDistanceToCamera, RPASS_ALPHA, alphaCustomFlags);
								pEntity->SetAddedToDrawListThisFrame(renderPhase->GetDrawListType());
							}
						}

						if (pEntity->IsBaseFlagSet(fwEntity::HAS_DISPL_ALPHA) && (phaseMask&canRenderAlpha))
						{
							rlDesc.AddEntity(pEntity, subphaseVisFlags, customDistanceToCamera, RPASS_ALPHA, alphaCustomFlags);
							pEntity->SetAddedToDrawListThisFrame(renderPhase->GetDrawListType());
						}

						if (pEntity->IsBaseFlagSet(fwEntity::HAS_WATER) && (phaseMask&canRenderWater))
						{
							rlDesc.AddEntity(pEntity, subphaseVisFlags, customDistanceToCamera, RPASS_WATER, baseCustomFlags);
							pEntity->SetAddedToDrawListThisFrame(renderPhase->GetDrawListType());
						}

						if(pEntity->IsBaseFlagSet(fwEntity::HAS_DECAL) && (phaseMask&canRenderDecal) && bShouldDoDecal)
						{
							rlDesc.AddEntity(pEntity, subphaseVisFlags, customDistanceToCamera, RPASS_DECAL, customFlags);
							pEntity->SetAddedToDrawListThisFrame(renderPhase->GetDrawListType());
						}	

						if (pEntity->IsBaseFlagSet(fwEntity::HAS_CUTOUT) && (phaseMask&canRenderCutout) && bShouldDoCutout)
						{
							rlDesc.AddEntity(pEntity, subphaseVisFlags, customDistanceToCamera, RPASS_CUTOUT, customFlags);
							pEntity->SetAddedToDrawListThisFrame(renderPhase->GetDrawListType());
						}
					}
				}
			}
		}

		pPvsEntry++;
	}
}

void CPostScan::StartAsyncSearchForAdditionalPreRenderEntities()
{
	PF_AUTO_PUSH_TIMEBAR("Start PreRender Searches");

	//////////////////////////////////////////////////////////////////////////
	// sphere around the player
	if ( g_scanDebugFlags & SCAN_DEBUG_ENABLE_PRERENDER_SPHERE )
	{
		Assertf(!ms_bAsyncSearchStarted, "Async search for additional PreRender entities already started");
		Vec3V						camPos = VECTOR3_TO_VEC3V( camInterface::GetPos() );

		// Vehicles
		{
			const spdSphere			preRenderSphere( camPos, ScalarV(PRERENDER_VEHICLE_SPHERE_RADIUS) );
			fwIsSphereIntersecting	testSphere( preRenderSphere );

			ms_PreRenderVehicleSearch.Start
				(
				fwWorldRepMulti::GetSceneGraphRoot(),
				&testSphere,
				ENTITY_TYPE_MASK_VEHICLE,
				SEARCH_LOCATION_EXTERIORS | SEARCH_LOCATION_INTERIORS,
				SEARCH_LODTYPE_HIGHDETAIL,
				SEARCH_OPTION_DYNAMICS_ONLY,
				WORLDREP_SEARCHMODULE_DEFAULT,
				sysDependency::kPriorityHigh
				);
		}

		// Peds & Objects
		{
			const spdSphere			preRenderSphere( camPos, ScalarV(PRERENDER_PED_AND_OBJECT_SPHERE_RADIUS) );
			fwIsSphereIntersecting	testSphere( preRenderSphere );

			ms_PreRenderPedAndObjectSearch.Start
				(
				fwWorldRepMulti::GetSceneGraphRoot(),
				&testSphere,
				ENTITY_TYPE_MASK_PED | ENTITY_TYPE_MASK_OBJECT,
				SEARCH_LOCATION_EXTERIORS | SEARCH_LOCATION_INTERIORS,
				SEARCH_LODTYPE_HIGHDETAIL,
				SEARCH_OPTION_DYNAMICS_ONLY,
				WORLDREP_SEARCHMODULE_DEFAULT,
				sysDependency::kPriorityHigh
				);
		}

		ms_bAsyncSearchStarted = true;
	}
}

void CPostScan::EndAsyncSearchForAdditionalPreRenderEntities()
{
	if ( g_scanDebugFlags & SCAN_DEBUG_ENABLE_PRERENDER_SPHERE )
	{
		ms_PreRenderVehicleSearch.Finalize();
		ms_PreRenderPedAndObjectSearch.Finalize();
		ms_bAsyncSearchStarted = false;
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	AddToAlwaysPreRenderList
// PURPOSE:		script may require that certain entities always be prerendered regardless of whether
//				they are visible or not. sets a flag in the entity and pushes it onto a linked list.
//////////////////////////////////////////////////////////////////////////
void CPostScan::AddToAlwaysPreRenderList(CEntity* pEntity)
{
	if ( pEntity && pEntity->IsBaseFlagSet(fwEntity::HAS_PRERENDER) && !pEntity->GetAlwaysPreRender() )
	{
		pEntity->SetAlwaysPreRender(true);
		m_alwaysPreRenderList.Add(pEntity);
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	RemoveFromAlwaysPreRenderList
// PURPOSE:		script may require that certain entities always be prerendered regardless of whether
//				they are visible or not. clears a flag in the entity and removes it from a linked list.
//////////////////////////////////////////////////////////////////////////
void CPostScan::RemoveFromAlwaysPreRenderList(CEntity* pEntity)
{
	if ( pEntity && Verifyf( pEntity->GetAlwaysPreRender(), "Trying to remove entity %s from \"always pre render\" list but it is not currently inserted", pEntity->GetModelName() ) )
	{
		pEntity->SetAlwaysPreRender(false);
		m_alwaysPreRenderList.Remove(pEntity);
	}
}

void CPostScan::AddToNonVisiblePreRenderList(CEntity* pEntity)
{
	Assert(pEntity);

	m_nonVisiblePreRenderList.AddEntity(pEntity);
}

void CPostScan::StartSortPVSJob()
{
	Assert(!g_SortPVSDependencyRunning);

	if(GetPVSBase() == GetPVSEnd())
	{
		return;
	}

	int minSortVal = MAX_INT32;
	int maxSortVal = MIN_INT32;
	for(int i = 0; i < LODTYPES_DEPTH_TOTAL; i++)
	{
		int sortVal = fwLodData::GetSortVal(i);
		minSortVal = sortVal < minSortVal ? sortVal : minSortVal;
		maxSortVal = sortVal > maxSortVal ? sortVal : maxSortVal;
	}

	g_SortPVSDependency.m_Params[0].m_AsPtr = GetPVSBase();
	g_SortPVSDependency.m_Params[1].m_AsPtr = &m_pCurrPVSEntry;
	g_SortPVSDependency.m_Params[2].m_AsPtr = &g_SortPVSDependencyRunning;
#if __BANK
	g_SortPVSDependency.m_Params[3].m_AsBool = CPostScanDebug::ShouldPartitionPvsInsteadOfSorting();
#else
	g_SortPVSDependency.m_Params[3].m_AsBool = true;
#endif
	g_SortPVSDependency.m_Params[4].m_AsInt = minSortVal;
	g_SortPVSDependency.m_Params[5].m_AsInt = maxSortVal;
	g_SortPVSDependency.m_Params[6].m_AsPtr = GetPVSBase();
	g_SortPVSDependency.m_DataSizes[0] = ptrdiff_t_to_int((u8*)GetPVSEnd() - (u8*)GetPVSBase());

	g_SortPVSDependencyRunning = 1;

	sysDependencyScheduler::Insert( &g_SortPVSDependency );
}

void CPostScan::WaitForSortPVSDependency()
{
	PF_PUSH_TIMEBAR("Wait For Sort PVS Dependency");
	while(g_SortPVSDependencyRunning)
	{
		sysIpcYield(PRIO_NORMAL);
	}
	PF_POP_TIMEBAR();
}

void CPostScan::WaitForPostScanHelper()
{
	PF_PUSH_TIMEBAR("Wait for BuildRenderListsFromPVS Dependency");
	while(g_BuildRenderListsFromPVSDependencyRunning)
	{
		sysIpcYield(PRIO_NORMAL);
	}
	PF_POP_TIMEBAR();
}

static bool sm_HasAlphaOverrideInfoBeenDumped = false;

bool CPostScan::AddAlphaOverride(CEntity *pEntity, int alpha  BANK_PARAM(const atString& ownerString) )
{
	int freeIdx = -1;
	
	for(int i=0;i<MAX_ALPHA_OVERRIDES;i++)
	{
		if( ms_alphaOverrides[i].entity == pEntity )
		{
			ms_alphaOverrides[i].alpha = alpha;
			return true;
		}
		else if( ms_alphaOverrides[i].entity == NULL )
		{
			freeIdx = i;
		}
	}
	
	if( freeIdx != -1 )
	{
		ms_alphaOverrides[freeIdx].entity = pEntity;
		ms_alphaOverrides[freeIdx].alpha = alpha;

#if __BANK
		ms_alphaOverrides[freeIdx].owner = ownerString;
#endif

		ms_hasAnyAlphaOverride = true;
		ms_lastAlphaOverride = Max(ms_lastAlphaOverride,freeIdx+1);
	}
#if __BANK
	else if(!sm_HasAlphaOverrideInfoBeenDumped)
	{
		sm_HasAlphaOverrideInfoBeenDumped = true;
		gnetDebug1("Failed to find free index for alpha override. Dumping array:");
		for(int i=0;i<MAX_ALPHA_OVERRIDES;i++)
		{
			gnetDebug1("Entity: %s with alpha: %d", ms_alphaOverrides[i].entity ? ms_alphaOverrides[i].entity->GetLogName() : "Empty", ms_alphaOverrides[i].alpha);
		}
	}
#endif

	return freeIdx != -1;
}


bool CPostScan::IsOnAlphaOverrideList(CEntity* pEntity)
{
	for(int i=0;i<MAX_ALPHA_OVERRIDES;i++)
	{
		if( ms_alphaOverrides[i].entity == pEntity )
		{
			return true;
		}
	}

	return false;
}

#if __BANK

const char* CPostScan::GetAlphaOverrideOwner(CEntity* pEntity)
{
	for(int i=0;i<MAX_ALPHA_OVERRIDES;i++)
	{
		if( ms_alphaOverrides[i].entity == pEntity )
		{
			return ms_alphaOverrides[i].owner.c_str();
		}
	}

	return NULL;
}

#endif

int CPostScan::GetAlphaOverride(CEntity *pEntity)
{
	for(int i=0;i<MAX_ALPHA_OVERRIDES;i++)
	{
		if( ms_alphaOverrides[i].entity == pEntity )
		{
			return ms_alphaOverrides[i].alpha;
		}
	}
	
	return pEntity->GetAlpha();
}

bool CPostScan::GetAlphaOverride(const CEntity *pEntity, int& alpha)
{
	alpha = 0;
	for(int i=0;i<MAX_ALPHA_OVERRIDES;i++)
	{
		if( ms_alphaOverrides[i].entity == pEntity )
		{
			alpha = ms_alphaOverrides[i].alpha;
			return true;
		}
	}

	return false;
}

void CPostScan::RemoveAlphaOverride(CEntity *pEntity)
{
	for(int i=0;i<MAX_ALPHA_OVERRIDES;i++)
	{
		if( ms_alphaOverrides[i].entity == pEntity )
		{
			ms_alphaOverrides[i].entity = NULL;
			ms_alphaOverrides[i].alpha = 0;
			return;
		}
	}
}

void CPostScan::AddEntitySortBias(CEntity *pEntity, float bias)
{
	int count = ms_sortBiases.GetCount();
	for(int i = 0; i < count; i++)
	{
		if(ms_sortBiases[i].entity == pEntity)
		{
			ms_sortBiases[i].bias = bias;
			return;
		}
	}

	entitySortBias& newBias = ms_sortBiases.Grow();
	newBias.entity = pEntity;
	newBias.bias = bias;
}

void CPostScan::RemoveEntitySortBias(CEntity *pEntity)
{
	for(int i = 0; i < ms_sortBiases.GetCount(); i++)
	{
		if(ms_sortBiases[i].entity == pEntity)
		{
			ms_sortBiases.DeleteFast(i);
			return;
		}
	}
}

void CPostScan::PossiblyAddToLateLightList(CEntity* pEntity)
{
	if (ms_bAllowLateLightEntities && pEntity && 
	    (CutSceneManager::GetInstance()->IsCutscenePlayingBack() || gStreamingRequestList.IsPlaybackActive()))
	{
		if(ms_LateLightsNearCameraSphere.IntersectsSphere(pEntity->GetBoundSphere()))
		{
			ms_lateLights.PushAndGrow(pEntity, 64);
		}
	}
}


bool BuildRenderListsFromPVS_Dependency(const sysDependency& )
{
	CSystem::SetThisThreadId(SYS_THREAD_POSTSCAN);

	PIXBegin(0, "PostScanHelper");
	gRenderListGroup.SetThreadAccessMask(SYS_THREAD_POSTSCAN);

	{
		PF_FUNC(BuildRenderLists);
		gPostScan.BuildRenderListsFromPVS();
	}

	{
		PF_FUNC(SortRenderLists);

		// Sorting will be done asynchronously - this loop will set up all of the job data
		atFixedArray<int, MAX_NUM_RENDER_PHASES> renderListsToSort;
		int count = RENDERPHASEMGR.GetRenderPhaseCount();
		for(int i=0;i<count;i++) 
		{
			fwRenderPhase &renderPhase = RENDERPHASEMGR.GetRenderPhase(i);

			if (renderPhase.HasEntityListIndex())
			{
				// make sure we only sort each list once
				int EntityListIndex = renderPhase.GetEntityListIndex();
				if(renderListsToSort.Find(EntityListIndex) == -1)
				{
					renderListsToSort.Push(EntityListIndex);
				}
			}
		}

		for(int i = 0; i < renderListsToSort.GetCount(); i++)
		{
			gRenderListGroup.GetRenderListForPhase(renderListsToSort[i]).PostRenderListBuild();
		}

#if SORT_RENDERLISTS_ON_SPU
		// This will kick off the sorting job - we block wait for the results near the end of CGame::Update()
		gRenderListGroup.StartSortEntityListJob();
#endif	//SORT_RENDERLISTS_ON_SPU
	}

	gRenderListGroup.SetThreadAccessMask(sysThreadType::THREAD_TYPE_UPDATE);
	CSystem::ClearThisThreadId(SYS_THREAD_POSTSCAN);

	g_BuildRenderListsFromPVSDependencyRunning = 0;

	PIXEnd();

	return true;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Process
// PURPOSE:		strip out specific exterior models from gbuf. assumed PVS is sorted by lod level.
//////////////////////////////////////////////////////////////////////////
void CPostScanExteriorHdCull::Process(CGtaPvsEntry* pStart, CGtaPvsEntry* pStop)
{
	if ( Unlikely(IsActive()) )
	{
		const u32 hdSortVal = fwLodData::GetSortVal(LODTYPES_DEPTH_HD);
		const u32 gbufIndex = g_SceneToGBufferPhaseNew->GetEntityListIndex();
		const u32 mirrorReflectionIndex = g_MirrorReflectionPhase->GetEntityListIndex();
		const u32 shadowIndex = g_CascadeShadowsNew->GetEntityListIndex();
		bool bStillProcessingHd = true;

		CGtaPvsEntry* pPvsEntry = (pStop - 1);
		while (bStillProcessingHd && pPvsEntry>=pStart)
		{
			if ( pPvsEntry && pPvsEntry->GetEntity()
				&& pPvsEntry->GetVisFlags().GetLodSortVal()==hdSortVal
				&& (pPvsEntry->GetVisFlags().GetOptionFlags()&VIS_FLAG_OPTION_INTERIOR)==0 )
			{
				fwVisibilityFlags& visFlags = pPvsEntry->GetVisFlags();

				CEntity* pEntity = pPvsEntry->GetEntity();
				if (ShouldCullEntity(pEntity))
				{		
					visFlags.ClearVisBit(gbufIndex);
					visFlags.ClearVisBit(mirrorReflectionIndex);
				}

				if (ShouldShadowCullEntity(pEntity))
				{
					visFlags.ClearVisBit(shadowIndex);
				}
				
			}

			if (pPvsEntry->GetEntity())
			{
				bStillProcessingHd = ( pPvsEntry->GetVisFlags().GetLodSortVal()==hdSortVal );
			}
			pPvsEntry--;
		}

		Init();
	}
}


bool CPostScanExteriorHdCull::ShouldCullEntity(CEntity* pEntity)
{
	if (pEntity)
	{
		// cull by sphere
		if (m_bCullSphereEnabled)
		{
			const spdAABB entityBox = pEntity->GetBoundBox();
			if (entityBox.IntersectsSphere(m_cullSphere))
				return true;
		}
		// cull by model name
		if (m_cullList.GetCount()>0)
		{
			CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
			if (pModelInfo)
			{
				const u32 modelNameHash = pModelInfo->GetModelNameHash();
				for (s32 i=0; i<m_cullList.GetCount(); i++)
				{
					if (m_cullList[i]==modelNameHash)
						return true;
				}
			}
		}	
	}
	return false;
}

bool CPostScanExteriorHdCull::ShouldShadowCullEntity(CEntity* pEntity)
{
	if (pEntity)
	{
		// shadow cull by model name
		if (m_shadowCullList.GetCount()>0)
		{
			CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
			if (pModelInfo)
			{
				const u32 modelNameHash = pModelInfo->GetModelNameHash();
				for (s32 i=0; i<m_shadowCullList.GetCount(); i++)
				{
					if (m_shadowCullList[i]==modelNameHash)
						return true;
				}
			}
		}	
	}
	return false;
}

#define CARRIER_GROUP_ID (100)

void ForceUpLinkedInteriors(fwInteriorLocation intLoc, CInteriorInst *pIntInst, const spdTransposedPlaneSet8 &frustum, atArray<fwInteriorLocation> &visitedLocs)
{
	if (!pIntInst)
	{
		return;
	}

	// for not-tunnel linked interiors, or the carrier (because it is so _special_) - bug 2016877
	bool bGroupTest = pIntInst->GetProxy()->GetGroupId() == 0 ||  pIntInst->GetProxy()->GetGroupId() == CARRIER_GROUP_ID;

	if (bGroupTest && pIntInst->GetProxy()->GetCurrentState() >= CInteriorProxy::PS_FULL)
	{
		s32 roomIndex = intLoc.GetRoomIndex();
		for(u32 i = 0; i < pIntInst->GetNumPortalsInRoom(roomIndex); ++i)
		{
			if (pIntInst->IsMirrorPortal(roomIndex, i))
			{
				continue;
			}

			if (pIntInst->IsLinkPortal(roomIndex, i))
			{
				s32 portalIdx = pIntInst->GetPortalIdxInRoom(roomIndex, i);
				const fwPortalCorners &portalCorners = pIntInst->GetPortal(portalIdx);
				spdAABB aabb;
				portalCorners.CalcBoundingBox(aabb);

				if (frustum.IntersectsAABB(aabb))
				{
					CPortalInst* pPortalInst = pIntInst->GetMatchingPortalInst(roomIndex,i);
					if (Verifyf(pPortalInst, "Failed to get pPortalInst"))
					{
						// update using info from the linked portal
						CInteriorInst* pDestIntInst = NULL;
						s32	destPortalIdx = -1;

						// get data about linked interior through this portal 
						pPortalInst->GetDestinationData(pIntInst, pDestIntInst, destPortalIdx);
						if (pDestIntInst) 
						{
							if (pDestIntInst->SetAlphaForRoomZeroModels(LODTYPES_ALPHA_VISIBLE))
							{
								pDestIntInst->SuppressInternalLOD();

								fwInteriorLocation linkLoc;
								s32 destRoomIdx = pDestIntInst->GetDestThruPortalInRoom(0, destPortalIdx);


								if (destRoomIdx > 0)
								{
									linkLoc.SetInteriorProxyIndex(pDestIntInst->GetProxy()->GetInteriorProxyPoolIdx());
									linkLoc.SetRoomIndex(destRoomIdx);
									bool alreadyVisited = false;
									for (int j = 0; j < visitedLocs.GetCount(); ++j)
									{
										if (linkLoc.GetAsUint32() == visitedLocs[j].GetAsUint32())
										{
											alreadyVisited = true;
											break;
										}
									}
									if (!alreadyVisited)
									{
										if (visitedLocs.GetCount() < visitedLocs.GetCapacity())
										{
											visitedLocs.Append() = intLoc;
											ForceUpLinkedInteriors(linkLoc, pDestIntInst, frustum, visitedLocs);
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
