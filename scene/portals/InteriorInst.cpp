// Title	:	InteriorInst.cpp
// Author	:	John Whyte
// Started	:	19/10/2005

#include "scene/portals/InteriorInst.h"

// Rage headers
#include "diag/channel.h"
#include "diag/art_channel.h"

// Framework headers
#include "fwscene/interiors/PortalTypes.h"
#include "fwscene/search/SearchVolumes.h"
#include "fwscene/world/WorldRepMulti.h"
#include "fwscene/stores/maptypesstore.h"

// Game headers
#include "audio/northaudioengine.h"
#include "camera/CamInterface.h"
#include "camera/replay/ReplayFreeCamera.h"
#include "camera/viewports/ViewportManager.h"
#include "control/replay/replay.h"
#include "core/game.h"
#include "cutscene/CutSceneManagerNew.h"
#include "modelinfo/MloModelInfo.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "network/general/NetworkUtil.h"
#include "objects/Door.h"
#include "objects/DummyObject.h"
#include "pathserver/ExportCollision.h"
#include "peds/Ped.h"
#include "Peds/PedFactory.h"
#include "pickups/PickupManager.h"
#include "Renderer/Lights/LightEntity.h"
#include "renderer/lights/lights.h"
#include "renderer/Mirrors.h"
#include "renderer/PostProcessFX.h"
#include "renderer/Shadows/Shadows.h"
#include "scene/InstancePriority.h"
#include "scene/loader/mapdata_entities.h"
#include "scene/loader/mapdata_interiors.h"
#include "scene/MapChange.h"
#include "scene/portals/interiorgroupmanager.h"
#include "scene/portals/portal.h"
#include "scene/portals/portalinst.h"
#include "scene/scene_channel.h"
#include "scene/world/gameworld.h"
#include "streaming/BoxStreamer.h"
#include "streaming/streaming.h"
#include "timecycle/TimeCycle.h"
#include "vehicles/VehicleFactory.h"
#include "vfx/systems/VfxWeapon.h"
#include "fwscene/scan/Scan.h"

FW_INSTANTIATE_CLASS_POOL_SPILLOVER(CInteriorInst, CONFIGURED_FROM_FILE, 0.44f, atHashString("InteriorInst",0x24852309));			// was 1000

SCENE_OPTIMISATIONS()


CPortalInst* CInteriorInst::ms_pLinkingPortal;		// used to hold result of link portal search call back
CInteriorInst* CInteriorInst::ms_pLockedForDeletion = NULL;
CInteriorInst* CInteriorInst::ms_pHoldRoomZeroInterior = NULL;
strRequestArray<INTLOC_MAX_ENTITIES_IN_ROOM_0>	CInteriorInst::ms_roomZeroRequests;

static CInteriorInst *pDebugInt = NULL;

extern bool bDisablePortalStream;

#if __BANK
bool gUsePortalTrackerCapture = true;
bool gForceClosablePortalsShutOnInit = true;
#else
const bool gForceClosablePortalsShutOnInit = true;
#endif

void CInteriorInst::CheckInteriorIntegrity(u32 modelIdx){

	if (pDebugInt == NULL)
	{
		CPed * pPlayer = CGameWorld::FindLocalPlayer();
		if (pPlayer)
		{
			if (pPlayer->GetPortalTracker())
			{
				pDebugInt = pPlayer->GetPortalTracker()->GetInteriorInst();
			}
		}
	}

	// look through buildings pool for instances of the factory
	if (pDebugInt && (pDebugInt->GetModelIndex() == modelIdx))			// factory
	{
		// go through all the tracked objects in this interior to test for integrity
		// count tracked objects in this interior
		u32 numCurrIntRooms = pDebugInt->GetNumRooms();
		for(u32 j=0;j<numCurrIntRooms;j++){
			fwPtrList& trackedObjList =  pDebugInt->GetTrackedObjects(j);
			fwPtrNode* pNode = trackedObjList.GetHeadPtr();

			while(pNode != NULL){
				CPortalTracker* pPT = static_cast<CPortalTracker*>(pNode->GetPtr());
				Assert(pPT);
				CInteriorInst* pIntInst = pPT->GetInteriorInst();
				if (pIntInst){
					Assert(pIntInst == pDebugInt);
					Assert(pPT->m_roomIdx == (int)j);
					if (pPT->m_pOwner){
						Assert(pPT->m_pOwner->GetInteriorLocation().GetRoomIndex() == (int)j);
					}
				}
				pNode = pNode->GetNextPtr();
			}
		}
	}
}

bool InteriorCollisionCB(CEntity* ASSERT_ONLY(pEntity), void* ASSERT_ONLY(data)){

	Assert(pEntity);
	Assert(data);

#if __ASSERT
	CEntity* pEntity2 = static_cast<CEntity*>(data);
	Vector3 pos = VEC3V_TO_VECTOR3(pEntity2->GetTransform().GetPosition());

	Assert(pEntity->GetBaseModelInfo());
	Assert(pEntity2->GetBaseModelInfo());

	// check if there overlap at mlo position, suggesting duplicate mlos
	if (pEntity && pEntity2)
	{
		Vector3 v = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()) - pos;
		float fDistance = v.Mag();
		if (fDistance < 1.0f)
		{
#if __ASSERT
			artLogf(DIAG_SEVERITY_ASSERT, "You have two interiors on top of each other: %s and %s at (%.0f,%.0f,%.0f)\n",
				pEntity->GetModelName(),
				pEntity2->GetModelName(), 
				pos.x,pos.y,pos.z);
#endif // __ASSERT

			return false;
		}
	}
#endif 

	return(true);
}

void CInteriorInst::CheckForCollisions(){

	// test all MLOs within possible collision distance of this MLO
	fwIsSphereIntersecting testSphere(spdSphere(GetTransform().GetPosition(), ScalarV(V_FLT_SMALL_1))); // r = 0.1f

	CGameWorld::ForAllEntitiesIntersecting(&testSphere, InteriorCollisionCB, this, ENTITY_TYPE_MASK_MLO, SEARCH_LOCATION_EXTERIORS, SEARCH_LODTYPE_HIGHDETAIL, SEARCH_OPTION_NONE, WORLDREP_SEARCHMODULE_PORTALS);
}

//
// name:		CPortal::CPortal
// description:	Constructor for a portal
CInteriorInst::CInteriorInst(const eEntityOwnedBy ownedBy)
	: CBuilding( ownedBy )
{
	SetTypeMLO();
	EnableCollision();

	m_bInUse = false;
	m_bIsDummied = true;		//starts off dummied
	m_bIsPopulated = false;

	m_bIsShellPopulated = false;
	m_bIsContentsPopulated = false;
	m_bIsLayoutPopulated = false;

	m_bArePortalsCreated = false;
	m_bInstanceDataGenerated = false;

	m_bResetObjFadeBecauseOfLowLOD = false;

	m_bIsAddedToLodProcessListForPhase = 0;

	m_bTriggerInstantFadeUp = false;
	m_bCutsceneFadeUp = false;

	m_bForceLowLODOn = false;

	m_roomData.Reset();

	RestartEntityInInteriorList();

	m_currDetailLevel = IILL_NONE;

	m_numExitPortals = 0;

	// need to store 2 distances - 1 for closest portal found when scanning through the world & 1 for closest
	// portal found when scanning linked interiors
	m_closestLinkedPortalDist = PORTAL_INSTANCE_DIST;
	m_closestExternPortalDist = 1000.0f;

	// Flag to always contain spawnpoints, for the spawn point entity iterators
	m_nFlags.bHasSpawnPoints = true;
	
	m_bHasCapturedFlag = false;

	m_MLODist = 10001.0f;		// make it obvious when a CInteriorInst has been constructed

	m_bDoNotRenderLowLODs = false;
	m_bDoNotRenderDetail = false;

	m_bContainsWater = false;
	m_bIsShuttingDown = false;

	m_bNeedsFullTransform = false;

	m_interiorSceneNode = NULL;

	m_pInternalLOD = NULL;

	m_pProxy = NULL;

	m_MLOFlags = 0;

	m_pShellRequests = NULL;
	m_openPortalsCount = 0;
	m_bAddedToClosedInteriorList = false;
	m_bInterfacePortalInstsCreated = false;
	m_bIgnoreInstancePriority = false;
}

CInteriorInst::~CInteriorInst()
{
	Assertf(!audNorthAudioEngine::IsAudioUpdateCurrentlyRunning(), "Deleting interior instance while audio is updating");

	m_bIsShuttingDown = true;

	CPortal::RemoveFromActiveInteriorList(this);

	m_MLODist = -10001.0f;		// make it obvious when a CInteriorInst has been destroyed

	Update();		// force PT consistency check before destruction

	// if we are deleting the primary tracker then it needs invalidating
	CPortalVisTracker::CheckForResetPrimaryData(this);

	GetProxy()->CleanupInteriorImmediately();

	if(audNorthAudioEngine::GetGtaEnvironment()->GetListenerInteriorInstance() == this)
	{
		audNorthAudioEngine::GetGtaEnvironment()->PurgeInteriors();
	}

	sceneAssert(!m_bIsShellPopulated);
	sceneAssert(!m_bIsContentsPopulated);
	Assert( m_interiorSceneNode == NULL );

	// destroy portal attached objects
	if (!m_bIsDummied){
		PortalObjectsConvertToDummy();
	}

	Update();		// force PT consistency check before destruction

	for(s32 i=0;i<m_roomData.GetCount();i++) {

#if __DEV
		fwPtrNode* pNode = m_roomData[i].m_activeTrackerList.GetHeadPtr();
		while(pNode != NULL){
			CPortalTracker* pPT =  static_cast<CPortalTracker*>(pNode->GetPtr());
			Assert(pPT != NULL);
			pNode = pNode->GetNextPtr();
		}
#endif //__DEV

		m_roomData[i].m_activeTrackerList.Flush();
	}
	m_roomData.Reset();

#if __DEV
	// check that no objects in the pool think they are still in this interior
	CObject::Pool* pObjectPool = CObject::GetPool();
	s32 			j = (s32) pObjectPool->GetSize();
	CObject*		pObject = NULL;

	while(j--)
	{
		pObject = pObjectPool->GetSlot(j);
		CInteriorInst* pIntInst = pObject ? pObject->GetPortalTracker()->GetInteriorInst() : NULL;
		if(pObject && pObject->GetPortalTracker() && pIntInst && 
			pIntInst->GetInteriorLocation().GetRoomIndex() != INTLOC_ROOMINDEX_LIMBO && 
			pIntInst->GetInteriorLocation().GetRoomIndex() != INTLOC_INVALID_INDEX)
		{
			if (pIntInst == this){
				CBaseModelInfo* pMI = GetBaseModelInfo();
				Assert(pMI);
				CBaseModelInfo* pMI2 = pIntInst->GetBaseModelInfo();
				Assert(pMI2);
				Assertf(false, "Deleting %s, but %s still pointing into it\n",pMI->GetModelName(),pMI2->GetModelName());
			}
		}
	}
#endif //__DEV

	//detatch from proxy
	m_pProxy->SetInteriorInst(NULL);
	m_pProxy = NULL;
}

// search for available link portals very close to the one we are wanting
CPortalInst* CInteriorInst::FindExistingLinkPortal(u32 portalIdx){

	fwPortalCorners	cornerPts;
	GetPortalInRoom(0,portalIdx,cornerPts);

	Vec3V radiusVec = VECTOR3_TO_VEC3V(cornerPts.GetCorner(0) - cornerPts.GetCorner(2))*ScalarV(V_HALF);
	Vec3V portalCentre = VECTOR3_TO_VEC3V(cornerPts.GetCorner(0)) - radiusVec;		//mid point of both points
	fwIsSphereIntersecting testSphere(spdSphere(portalCentre, ScalarV(V_HALF)));

	fwPtrListSingleLink	scanList;

	CPortalInst::FindIntersectingPortals( portalCentre, testSphere, scanList, true, false);			// scan through the portal pool looking for portal intersecting this sphere

	CPortalInst* pRetPortal = NULL;
	CPortalInst* pCurPortal = NULL;

	fwPtrNode* pNode = scanList.GetHeadPtr();

	while(pNode != NULL){
		pCurPortal = static_cast<CPortalInst*>(pNode->GetPtr());
		if (pCurPortal){
			Assert(pCurPortal->IsLinkPortal());

			// check centres are close enough to match (within 75 cms)
			Vec3V diff = pCurPortal->GetTransform().GetPosition() - portalCentre;
			if (MagSquared(diff).Getf() < 0.5625f){
#if __DEV
				Assertf(pRetPortal == NULL, "More than 2 link portals @ (%d,%d,%d)",(int)portalCentre.GetXf(),
																					(int)portalCentre.GetYf(),(int)portalCentre.GetZf());
				Assertf(!pCurPortal->HasBeenLinked(), "More than 2 link portals @ (%d,%d,%d)",(int)portalCentre.GetXf(), 
																					(int)portalCentre.GetYf(),(int)portalCentre.GetZf());
#endif //__DEV

				pRetPortal = pCurPortal;

#if NAVMESH_EXPORT
				if(CNavMeshDataExporter::IsExportingCollision() && pCurPortal->HasBeenLinked())
					pRetPortal = NULL;
#endif
			}
		}
		pNode = pNode->GetNextPtr();
	}

	return(pRetPortal);
}

void	CInteriorInst::CreateEntity(fwEntityDef* pEntityDef, fwInteriorLocation interiorLocation, bool bIntersectsModelSwap, bool bAddToShellRequests, u8 tintIdx)
{
	sceneAssert(m_pShellRequests);

	u32 priorityVal = pEntityDef->m_priorityLevel;
	if (!interiorLocation.IsAttachedToPortal() && priorityVal > CInstancePriority::GetCurrentPriority() && !m_bIgnoreInstancePriority)
	{
		return;
	}

	fwModelId modelId;
	CBaseModelInfo* pEntityModelInfo = CModelInfo::GetFullBaseModelInfoFromName(pEntityDef->m_archetypeName, &modelId);

	if (!artVerifyf( pEntityModelInfo!=NULL, "Interior <%s> contains unknown model <%s>. Skipping it.",GetMloModelInfo()->GetModelName(), pEntityDef->m_archetypeName.GetCStr()))
	{
		return;
	}

	bool isInstancingRequired = true;

	if (isInstancingRequired)
	{
		CEntity* pEntity = static_cast<CEntity*>(fwMapData::ConstructEntity(pEntityDef, GetIplIndex(), pEntityModelInfo, modelId));

		if(pEntity && interiorLocation.IsValid()) 
		{
			if (bAddToShellRequests)
			{
				m_pShellRequests->PushRequest(pEntityModelInfo->GetStreamSlot().Get(), CModelInfo::GetStreamingModuleId(), STRFLAG_DONTDELETE|STRFLAG_INTERIOR_REQUIRED);
			}

			if (pEntity->GetBaseModelInfo()->GetUsesDoorPhysics())
			{
				CDoor::AddDoorToList(pEntity);
			}

			const fwTransform*	pTransform = GetTransformPtr();
			Mat34V mloWorldMatrix = pTransform->GetMatrix();

			if ( !m_bNeedsFullTransform )
			{
				// this assumes that the interior will be a simple transform...
				if ( pEntity->GetTransform().IsSimpleTransform())
				{
					Vec3V entityPos = pEntity->GetTransform().GetPosition();
					Vec3V worldEntityPos = Transform(mloWorldMatrix, entityPos);
					pEntity->SetPosition(RCC_VECTOR3(worldEntityPos), true, false, false);
					float worldEntityHeading = pEntity->GetTransform().GetHeading() + pTransform->GetHeading();
					pEntity->SetHeading(worldEntityHeading);
				} else {
					Mat34V worldEntityMatrix;
					Transform(worldEntityMatrix, mloWorldMatrix, pEntity->GetTransform().GetNonOrthoMatrix());
					pEntity->SetMatrix(RCC_MATRIX34(worldEntityMatrix), true, false, false);
				}
			} else 
			{
				// not a simple transform, so entity is going to need a full matrix regardless
				Mat34V worldEntityMatrix;
				Mat34V localEntityMatrix = pEntity->GetTransform().GetNonOrthoMatrix();
				Transform(worldEntityMatrix, mloWorldMatrix, localEntityMatrix);

				#if ENABLE_MATRIX_MEMBER				
				pEntity->SetTransform(worldEntityMatrix);
				pEntity->SetTransformScale(1.0f, 1.0f);
				#else
				fwMatrixTransform* newMatrix = rage_new fwMatrixTransform(worldEntityMatrix);
				pEntity->SetTransform(newMatrix);
				#endif
			}

			pEntity->SetOwnedBy(ENTITY_OWNEDBY_INTERIOR);
			pEntity->SetBaseFlag(fwEntity::USE_SCREENDOOR);
			pEntity->SetTintIndex(tintIdx);

			if (bIntersectsModelSwap)
			{
				g_MapChangeMgr.ApplyChangesToEntity(pEntity);
			}

			// for LODed interiors : room 0 objects fade distance pushed to MLO fade dist, init alpha to 0 to avoid supressing LOD
 			if (m_pInternalLOD && pEntity != m_pInternalLOD && interiorLocation.IsAttachedToRoom() && interiorLocation.GetRoomIndex() == 0){
				pEntity->SetLodDistance(GetLodDistance());
 				pEntity->SetAlpha(0);
 			}

			// [HACK GTAV] - model with too short fade distance in one interior
			const u32 interiorNameHash = GetMloModelInfo()->GetModelNameHash();
			if (interiorNameHash == ATSTRINGHASH("v_garagel", 0x9bd1c95d))
			{
				const u32 objectNameHash = pEntity->GetBaseModelInfo()->GetModelNameHash();
				if (objectNameHash == ATSTRINGHASH("v_ilev_garageliftdoor", 0xb614b4ef ))
				{
					pEntity->SetLodDistance(30);
				}
			}

			if (interiorNameHash == ATSTRINGHASH("v_garages", 0x5988c45c))
			{
				const u32 objectNameHash = pEntity->GetBaseModelInfo()->GetModelNameHash();
				if (objectNameHash == ATSTRINGHASH("v_ilev_rc_door2", 0x875ff540 ))
				{
					Vector3 pos = VEC3V_TO_VECTOR3(pEntity->GetMatrix().d());

					if (pos.x < 179.0f)
					{
						pos.x += 0.01f;
					} else
					{
						pos.x -= 0.006f;
					}

					pEntity->SetPosition(pos, false, false, false);
				}
			}

#if GTA_REPLAY
			if(CReplayMgr::IsReplayInControlOfWorld() && pEntity->GetIsTypeBuilding())
			{
				CBuilding* pBuilding = (CBuilding*)pEntity;
				CReplayMgr::AddBuilding(pBuilding);

 				if(CReplayMgr::ShouldForceHideEntity(pEntity))
 				{
 					// Use the script module here....this shouldn't be used at all during replay
 					// and using the Replay module would be vulnerable to being overridden
 					pEntity->SetIsVisibleForModule(SETISVISIBLE_MODULE_SCRIPT,false,false);
 				}
			}
#endif // GTA_REPLAY
// 
// 			if (interiorNameHash == ATSTRINGHASH("v_genbank", 0x4fcb840e))
// 			{
// 				const u32 objectNameHash = pEntity->GetBaseModelInfo()->GetModelNameHash();
// 				if (objectNameHash == ATSTRINGHASH("v_10_gan_bank_reflect", 0x1b206ae6 ))
// 				{
// 					pEntity->GetRenderPhaseVisibilityMask().ClearFlag( VIS_PHASE_MASK_PARABOLOID_REFLECTION );
// 					pEntity->RequestUpdateInWorld();
// 				}
// 			}
			// [end HACK GTAV]

			CGameWorld::Add(pEntity, interiorLocation);	
		}
	}
}

void CInteriorInst::PopulateMloShell()	
{
	Assert(m_bIsShellPopulated == false);

	CMloModelInfo*	pMloModelInfo = GetMloModelInfo();
	sceneAssert( pMloModelInfo );
	sceneAssert(pMloModelInfo->GetIsStreamType());

	m_pShellRequests = rage_new strRequestArray<MAX_SHELL_ENTITIES>;
	sceneAssert(m_pShellRequests);

	if (!m_pShellRequests)
	{
		return;
	}

	// - create the internal LOD for this interior (if it has one) -

	CEntity* pLod = (CEntity*)GetLod();
	if (pLod)
	{
		const fwArchetype* pArchetype = pLod->GetArchetype();
		if (artVerifyf(pArchetype!=NULL, "Cannot find interior LOD model %s", pLod->GetModelName()))
		{
			fwModelId modelId;
			modelId = fwArchetypeManager::LookupModelId(pArchetype);

			CEntity* pEntity = static_cast<CEntity*>(const_cast<fwArchetype*>(pArchetype)->CreateEntity());
			if (pEntity)
			{ 
				pEntity->SetModelId(modelId);

				// set this new instance up with values copied from the original
				fwTransform* trans = NULL;
				Vec3V_Out pos = pLod->GetTransform().GetPosition();
				trans = rage_new fwSimpleTransform(pos.GetXf(), pos.GetYf(), pos.GetZf(), pLod->GetTransform().GetHeading());
#if ENABLE_MATRIX_MEMBER
				Mat34V m = trans->GetMatrix();
				pEntity->SetTransform(m);
				pEntity->SetTransformScale(1.0f, 1.0f);
				trans->Delete();
#else
				pEntity->SetTransform(trans);
#endif
				pEntity->GetLodData().SetLodType(LODTYPES_DEPTH_LOD);

				pLod->SetAlpha(LODTYPES_ALPHA_VISIBLE);

				const u32 interiorNameHash = pMloModelInfo->GetModelNameHash();
				u16 reflectionMask = VIS_PHASE_MASK_REFLECTIONS;

				// [HACK GTAV] -- don't clear mirror reflection for vbca tunnel
				if (interiorNameHash == ATSTRINGHASH("vbca_tunnel1", 0xB7B6F801) ||
					interiorNameHash == ATSTRINGHASH("vbca_tunnel2", 0x667E558D) ||
					interiorNameHash == ATSTRINGHASH("vbca_tunnel3", 0x5A6EBD6E) ||
					interiorNameHash == ATSTRINGHASH("vbca_tunnel4", 0x1271AD75) ||
					interiorNameHash == ATSTRINGHASH("vbca_tunnel5", 0xFFBC080A))
				{
					reflectionMask = VIS_PHASE_MASK_PARABOLOID_REFLECTION;
				}

				// don't draw any LOD in reflections
				pEntity->GetRenderPhaseVisibilityMask().ClearFlag( reflectionMask );
				pLod->GetRenderPhaseVisibilityMask().ClearFlag( reflectionMask );

				//[HACK GTAV] - broken LOD setup
				if (interiorNameHash == ATSTRINGHASH("v_gun2", 0x985c1f00))
				{
					pEntity->GetRenderPhaseVisibilityMask().ClearAllFlags();
				}

				fwInteriorLocation thisRoomZero = CInteriorProxy::CreateLocation(this->GetProxy(), 0);
				CGameWorld::Add(pEntity, thisRoomZero);	

				m_pInternalLOD = pEntity;

				m_pInternalLOD->CreateDrawable();		// because it may have lights which need added too.
			}
		}
	}

	// - create the entities which form the contents of this interior (all the default objects) -

	spdSphere			boundingSphere;
	GetBoundSphere(boundingSphere);
	const bool			bIntersectsModelSwap = g_MapChangeMgr.IntersectsWithActiveChange(boundingSphere);

	CreateInteriorEntities(pMloModelInfo, true, bIntersectsModelSwap);

	m_bIsShellPopulated = true;
}

void CInteriorInst::PopulateMloContents()			
{
	if (m_bIsPopulated){
		return;
	}

	Assert(m_bIsContentsPopulated == false);

	CMloModelInfo*	pMloModelInfo = GetMloModelInfo();
	sceneAssert( pMloModelInfo );
	sceneAssert(pMloModelInfo->GetIsStreamType());
	sceneAssert(m_bIsPopulated == false);

	// - create the entities which form the contents of this interior (all the default objects) -

	spdSphere			boundingSphere;
	GetBoundSphere(boundingSphere);
	const bool			bIntersectsModelSwap = g_MapChangeMgr.IntersectsWithActiveChange(boundingSphere);

	CreateInteriorEntities(pMloModelInfo, false, bIntersectsModelSwap);

	// - add in any active entity sets for this interior -

	CInteriorProxy* pProxy = GetProxy();
	Assert(pProxy);
	pProxy->ValidateActiveSets();

	for(u32 i = 0; i<pMloModelInfo->GetEntitySets().GetCount(); i++)
	{
		const CMloEntitySet& entitySet = pMloModelInfo->GetEntitySets()[i];
		if (pProxy->IsEntitySetActive(entitySet.m_name))
		{
			u32 bMatchingCount = (entitySet.m_entities.GetCount() == entitySet.m_entities.GetCount());
			sceneAssertf(bMatchingCount, "entity set (%s) in %s has bad data", entitySet.m_name.GetCStr(), pMloModelInfo->GetModelName());

			if (bMatchingCount)
			{
				UInt32 tintIdx = pProxy->GetEntitySetTintIndex(entitySet.m_name);
				for(u32 j = 0; j<entitySet.m_entities.GetCount(); j++)
				{
					u32 locationId = entitySet.m_locations[j];
					fwInteriorLocation entityLocation = CInteriorProxy::CreateLocation( pProxy, INTLOC_INVALID_INDEX);

					if ((locationId & (1<<31)) == 0)
					{
						entityLocation.SetRoomIndex(locationId);
					} 
					else 
					{
						entityLocation.SetPortalIndex(locationId & ~(1<<31));
					} 

					fwEntityDef* pEntityDef = entitySet.m_entities[j];
					
					CreateEntity(pEntityDef, entityLocation, bIntersectsModelSwap, false, (u8)tintIdx);
				}
			}
		}
	}

	// Add all the interior's timecycle modifiers. These are sphere modifiers, but for simplicity 
	// of storage, API, etc., we add them add the sphere as the encompassing AABB, with a bit set 
	// to let it know its really a sphere for calculation purposes.
	const fwTransform* pTransform = this->GetTransformPtr();
	Assertf(pTransform, "Interior transform is NULL, interior timecycle modifiers will not be placed correctly!");
	u32 numMloTimeCycleModifiers = pMloModelInfo->GetTimeCycleModifiers().GetCount();
	for(u32 i = 0; i < numMloTimeCycleModifiers; i++ )
	{
		const CMloTimeCycleModifier& mloTimeCycleModifier = pMloModelInfo->GetTimeCycleModifiers()[i];

		spdAABB aabb;
		aabb.SetAsSphereV4(VECTOR4_TO_VEC4V(mloTimeCycleModifier.m_sphere));
		if(pTransform)
		{
			aabb.Transform(pTransform->GetMatrix());
		}

		const char* debugTxt = NULL;
#if __DEV
		char msg[512];
		const char* interiorName = NULL;
#if !__NO_OUTPUT
		interiorName = pMloModelInfo->GetModelName();
#endif
		sprintf(msg,"%s - %s", interiorName, mloTimeCycleModifier.m_name.GetCStr());
		debugTxt = msg;
#endif
		g_timeCycle.AddModifierBox(
			aabb,
			mloTimeCycleModifier.m_name.GetHash(),
			mloTimeCycleModifier.m_percentage,
			mloTimeCycleModifier.m_range,
			mloTimeCycleModifier.m_startHour,
			mloTimeCycleModifier.m_endHour,
			m_pProxy->GetInteriorProxyPoolIdx(),
			debugTxt, 
			true, // true = treat as sphere modifier. See comment above.
			true // true = comes from interior inst
			);
	}

	m_bIsPopulated = true;
	m_bIsContentsPopulated = true;
}

// create a set of interior entities
// bSelectShell == true : room 0 and exterior portal attached entities
// bSelectShell == false : everything except for room 0 and exterior portal attached entities
void CInteriorInst::CreateInteriorEntities(CMloModelInfo* pMI, bool bSelectShell, bool bIntersectsModelSwap)
{
	Assert(pMI);

	// we want to determine the location of every default object in the interior - for later creating and placing them
	atArray<fwInteriorLocation>		contentsLocations;
	u32 numEntities = pMI->GetEntities().GetCount();
	contentsLocations.Reserve(numEntities);
	contentsLocations.Resize(numEntities);

	for(u32 i = 0; i<pMI->GetRooms().GetCount(); i++)
	{
		bool bAddThisRoom;

		if (bSelectShell)
		{
			bAddThisRoom = (i == 0);
		} else
		{
			bAddThisRoom = (i > 0);
		}
		
		if (bAddThisRoom)
		{
			for(u32 j = 0; j< pMI->GetRooms()[i].m_attachedObjects.GetCount(); j++)
			{
				const atArray<CMloRoomDef>& mloRooms = pMI->GetRooms();
				const atArray<u32>&	attachedObjects = mloRooms[i].m_attachedObjects;
				u32 entityIdx = attachedObjects[j];
				//Assert(entityIdx < numEntities);

				if(entityIdx != 0xffffffff)
				{
					fwInteriorLocation& entityLocation = contentsLocations[entityIdx];
					entityLocation = CInteriorProxy::CreateLocation( GetProxy(), INTLOC_INVALID_INDEX);
					entityLocation.SetRoomIndex(i);
				}
			}
		}
	}

	for(u32 i = 0; i<pMI->GetPortals().GetCount(); i++)
	{
		bool bAddThisPortal;
		const CMloPortalDef& portal = pMI->GetPortals()[i];

		if (bSelectShell)
		{
			bAddThisPortal = (portal.m_roomFrom == 0) || (portal.m_roomTo == 0);
		} else
		{
			bAddThisPortal = (portal.m_roomFrom != 0) && (portal.m_roomTo != 0);
		}

		if (bAddThisPortal)
		{
			for(u32 j = 0; j< pMI->GetPortals()[i].m_attachedObjects.GetCount(); j++)
			{
				const atArray<CMloPortalDef>& mloPortals = pMI->GetPortals();
				const atArray<u32>&	attachedObjects = mloPortals[i].m_attachedObjects;
				u32 entityIdx = attachedObjects[j];
				//Assert(entityIdx < numEntities);

				fwInteriorLocation& entityLocation = contentsLocations[entityIdx];
				entityLocation = CInteriorProxy::CreateLocation( GetProxy(), INTLOC_INVALID_INDEX);
				entityLocation.SetPortalIndex(i);
			}
		}
	}

	for(s32 i = 0; i<pMI->GetEntities().GetCount(); i++)
	{
		if (contentsLocations[i].IsValid())
		{
			CreateEntity(pMI->GetEntities()[i], contentsLocations[i], bIntersectsModelSwap, bSelectShell, 0);
		} 
	}
}

s32 FindGlobalPortalIdx(CMloModelInfo *pModelInfo, u32 roomIdx, u32 roomPortalIdx)
{
	for(u32 i=0; i<pModelInfo->GetPortals().GetCount(); i++)
	{
		const CMloPortalDef& portal = pModelInfo->GetPortals()[i];
		if (portal.m_roomFrom == roomIdx || portal.m_roomTo == roomIdx)
		{
			if (roomPortalIdx == 0)
			{
				return(i);
			} else
			{
				roomPortalIdx--;
			}
		}
	}

	return(-1);
}

bool	CInteriorInst::HasShellLoaded()
{
	sceneAssert(m_bIsShellPopulated);
	sceneAssert(m_pShellRequests);

	if (m_pShellRequests)
	{
		return(m_pShellRequests->HaveAllLoaded());
	}

	return(false);
}

// create all of the objects required by this interior (entities stored inside the interior & the portal objects connecting it)
bool	CInteriorInst::PopulateInteriorShell(void){

	sceneAssertf(GetProxy()->GetCurrentState() == CInteriorProxy::PS_NONE, "Interior proxy not in correct state for populating");

	if (GetProxy()->GetCurrentState() != CInteriorProxy::PS_NONE)
	{
		return(false);
	}

	if (m_bIsShellPopulated)
	{
		return(false);
	}

#if __BANK
	if (bDisablePortalStream)
	{
		return(false);
	}
#endif //__BANK

	GenerateInstanceData();
	InitializeSubSceneGraph();
	PopulateMloShell();
	if (!GetProxy()->GetIsCappedAtPartial())
	{
		CreateInterfacePortalInsts();
	}
	ProcessCloseablePortals();

	return(true);
}

// create all of the objects required by this interior (entities stored inside the interior & the portal objects connecting it)
bool	CInteriorInst::PopulateInteriorContents(void){

	sceneAssertf(GetProxy()->GetCurrentState() == CInteriorProxy::PS_PARTIAL, "Interior proxy not in correct state for populating");

	if (m_bIsContentsPopulated)
	{
		Assert(false);
		return(false);
	}

	PopulateMloContents();
	if (!m_bInterfacePortalInstsCreated)
	{
		CreateInterfacePortalInsts();
	}

	CEntity* pParent = (CEntity*)GetLod();
	if (pParent)
	{
		const u32 interiorNameHash = GetMloModelInfo()->GetModelNameHash();
		u16 reflectionMask = VIS_PHASE_MASK_REFLECTIONS;

		// [HACK GTAV] -- don't clear mirror reflection for vbca tunnel
		if (interiorNameHash == ATSTRINGHASH("vbca_tunnel1", 0xB7B6F801) ||
			interiorNameHash == ATSTRINGHASH("vbca_tunnel2", 0x667E558D) ||
			interiorNameHash == ATSTRINGHASH("vbca_tunnel3", 0x5A6EBD6E) ||
			interiorNameHash == ATSTRINGHASH("vbca_tunnel4", 0x1271AD75) ||
			interiorNameHash == ATSTRINGHASH("vbca_tunnel5", 0xFFBC080A))
		{
			reflectionMask = VIS_PHASE_MASK_PARABOLOID_REFLECTION;
		}

		pParent->GetRenderPhaseVisibilityMask().ClearFlag( reflectionMask );
		pParent->RequestUpdateInWorld();
	}
	
	SetAudioInteriorData();

	m_bIsPopulated = true;
	return(true);
}

void CInteriorInst::CreateInterfacePortalInsts()
{
	// if it doesn't contain any exterior portals then don't try to instance them
	u32 numRoomZeroPortals = GetNumPortalsInRoom(0);			// all entry / exit portals are in room 0
	if (numRoomZeroPortals == 0)
	{
		return;
	}

	// create the portal objects for this interior (which exist in the world & interface between world <-> interior or interior <-> interior
	CMloModelInfo *pModelInfo = GetMloModelInfo();
	Assert(pModelInfo);

	m_aRoomZeroPortalInsts.Reset();
	m_aRoomZeroPortalInsts.Resize(numRoomZeroPortals);
	for(u32 i=0; i<numRoomZeroPortals; i++)
	{
		m_aRoomZeroPortalInsts[i] = 0;
	}

	CPortalInst*	pPortalInst = NULL;

	s32 entryExitPortalIndex = -1;
	// instance out all entry /exit portals
	for(u32 interiorPortalIdx=0; interiorPortalIdx<pModelInfo->GetPortals().GetCount(); interiorPortalIdx++)
	{
		const CMloPortalDef& portal = pModelInfo->GetPortals()[interiorPortalIdx];
		if ((portal.m_roomTo != 0) && (portal.m_roomFrom !=0))
		{
			continue;
		}

		// temp - need room relative portal indices
		entryExitPortalIndex++;
		Assert(entryExitPortalIndex < numRoomZeroPortals);

		fwPortalCorners	cornerPts = m_aPortals[interiorPortalIdx]->GetCorners();
		CPortalFlags portalFlags = pModelInfo->GetPortalFlags(interiorPortalIdx);

		pPortalInst = NULL;

		// TODO : deal with link portals correctly (need to pickup flag correctly)
		// if this is a link portal then try to find an existing portal instance which is available to link to
		if (portalFlags.GetIsLinkPortal())
		{
			pPortalInst = FindExistingLinkPortal(entryExitPortalIndex);

			if (pPortalInst)
			{
				pPortalInst->SetLinkedInterior(this, entryExitPortalIndex);		// set linked interior data for discovered portal

				// merge linked portals corner points together so no gaps between them (there is always a degree of error in the map alignment)
				CInteriorInst*	primaryInt = pPortalInst->GetPrimaryInterior();
				CInteriorInst*	linkedInt = pPortalInst->GetLinkedInterior();

				Assert(primaryInt != this);
				Assert(linkedInt == this);
				MergePortalCorners(entryExitPortalIndex, pPortalInst);

				u32 linkedGlobalPortalIndex = linkedInt->GetPortalIdxInRoom( 0, pPortalInst->GetLinkedPortalIndex() );
				u32 primaryGlobalPortalIndex = primaryInt->GetPortalIdxInRoom( 0, pPortalInst->GetPrimaryPortalIndex() );

				fwPortalSceneGraphNode*		linkedPortal = linkedInt->m_aPortals[ linkedGlobalPortalIndex ];
				fwPortalSceneGraphNode*		primaryPortal = primaryInt->m_aPortals[ primaryGlobalPortalIndex ];

				linkedPortal->SetPositivePortalEnd( primaryPortal->GetNegativePortalEnd() );
				primaryPortal->SetPositivePortalEnd( linkedPortal->GetNegativePortalEnd() );
			}
		}	

		// we don't have an existing portal instance so we need to create one
		if (pPortalInst == NULL) 
		{
			// couldn't find an existing link portal to link to, so create a new one at this position
			pPortalInst = rage_new CPortalInst ( ENTITY_OWNEDBY_INTERIOR, cornerPts,this,entryExitPortalIndex); // create portal & set primary interior data
			Assert(pPortalInst);

			pPortalInst->SetDetailDistance(float(GetLodDistance())); //pIntInfo->GetInteriorDetailDist());

			if (portalFlags.GetIsLinkPortal())
			{
				pPortalInst->SetIsLinkPortal(true);
			}

			if (portalFlags.GetIgnoreModifier())
			{
				pPortalInst->SetIgnoreModifier(true);
			}

			if (portalFlags.GetUseLightBleed() )
			{
				pPortalInst->SetUseLightBleed(true);
			}

			pPortalInst->SetModelId(CPortal::GetPortalInstModelId());		// use specific model idx...
			CGameWorld::Add(pPortalInst, CGameWorld::OUTSIDE );
		}

		// store the ptr to either the newly created portal inst
		Assert(m_aRoomZeroPortalInsts[entryExitPortalIndex] == NULL);
		m_aRoomZeroPortalInsts[entryExitPortalIndex] = (pPortalInst);
	}

	m_bInUse = true;

	Assert( m_aRoomZeroPortalInsts.GetCount() > 0);

	CPortal::ms_interiorPopulationScanCode++;

	visibilityDebugf1("CPortal::ms_interiorPopulationScanCode incremented to %d by CInteriorInst::CreateInterfacePortalInsts", CPortal::ms_interiorPopulationScanCode);

	m_bInterfacePortalInstsCreated = true;
}

void	CInteriorInst::SetAudioInteriorData()
{
	CMloModelInfo *pModelInfo = GetMloModelInfo();
	if(sceneVerifyf(pModelInfo, "NULL CMLOModelInfo"))
	{
		for(s32 i=0;i<m_roomData.GetCount();i++)
		{			
			CalcRoomBoundBox(i, m_roomData[i].m_boundingBox);
			const InteriorSettings* intSettings = NULL;
			const InteriorRoom* intRoom = NULL;
			audNorthAudioEngine::GetGtaEnvironment()->FindInteriorSettingsForRoom(pModelInfo->GetHashKey(), GetRoomHashcode(i), intSettings, intRoom);
			pModelInfo->SetAudioSettings(i, intSettings, intRoom);
		}
	}
};

// remove all of the objects inside this interior (delete ones created by this interior, shove everything else in a stasis list)
bool	CInteriorInst::DepopulateInterior(void)
{
	Assertf(!audNorthAudioEngine::IsAudioUpdateCurrentlyRunning(), "Depopulating interior instance while audio is updating");

	m_bHasCapturedFlag = false;		// so it can capture again if need be

	sceneAssertf(GetProxy()->GetCurrentState() == CInteriorProxy::PS_PARTIAL, "Interior not in a suitable state for depopulating");
	if (this == CPortalVisTracker::GetPrimaryInteriorInst() && !camInterface::IsFadedOut())
	{
		Warningf("%s being depopulated whilst being used for primary view!", GetModelName());
		Assertf(false, "%s being depopulated whilst being used for primary view!", GetModelName());
	}

#if __ASSERT
	CInteriorProxy*		pInteriorProxy = GetProxy();
	Assert(pInteriorProxy);
#endif //__ASSERT

	if (GetProxy()->GetCurrentState() != CInteriorProxy::PS_PARTIAL){
		return (false);
	}

	if (m_pShellRequests)
	{
		m_pShellRequests->ClearRequiredFlags(STRFLAG_DONTDELETE|STRFLAG_INTERIOR_REQUIRED);
		m_pShellRequests->ReleaseAll();

		delete m_pShellRequests;
		m_pShellRequests = NULL;
	}

	GetProxy()->ReleaseStaticBoundsFile();

	if (ms_pHoldRoomZeroInterior == this)
	{
		HoldRoomZeroModels(NULL);
	}

	CEntity* pEntity = NULL;

	// don't use this iterator during destruction of contents - it doesn't work!
	for(s32 i = 0; i < (s32) GetNumRooms(); i++)
	{
		bool	resetLoop = true;
		ASSERT_ONLY(u32	iterationCount = 0; )

		// Every time we remove a composite entity we have to reset the loop, since that entity may have
		// removed entities inside the interior (invalidating the entities array)
		while (resetLoop)
		{
			resetLoop = false;
			Assertf( ++iterationCount < 10000, "DepopulateInterior iterated more than 10000 times on the entity array. There's probably something wrong with the iteration logic." );

			atArray< fwEntity* >		entities;
			m_roomData[i].GetRoomSceneNode()->GetEntityContainer()->GetEntityArray( entities );

			for(s32 j = 0; j < entities.GetCount() && !resetLoop; ++j)
			{
				pEntity = static_cast< CEntity* >( entities[j] );
				Assert( pEntity->GetIsInInterior() );

				if(pEntity)
				{
#if __ASSERT
					if (iterationCount > 10000){
						Displayf("---");
						Displayf("Emergency interior dump...\n");
						Displayf("interior : %s, room : %d, entity: %d\n",GetModelName(),i,j);
						CDebug::DumpEntity(pEntity);
					}
#endif //__ASSERT
					Assert(	pEntity->GetInteriorLocation().GetInteriorProxyIndex() == CInteriorProxy::GetPool()->GetJustIndex(this->GetProxy()));

					//fwInteriorLocation loc = pEntity->GetInteriorLocation();				

					if ( pEntity->GetIsTypeComposite() )
						resetLoop = true;

					if ( pEntity->GetIsTypeVehicle() || pEntity->GetIsTypePed() || pEntity->GetIsTypeObject() )
					{
						if (static_cast< CPhysical* >( pEntity )->GetChildAttachment() )
						{
							resetLoop = true;
						}
					}

					// any lights on this entity are going to end up stripped off of the object. Script owned entities might be getting pushed back in again which will require regenerating the lights
					if (GetProxy() && GetProxy()->GetIsRegenLightsOnAdd())
					{
						if (pEntity->GetOwnedBy() == ENTITY_OWNEDBY_SCRIPT)
						{
							LightEntityMgr::RemoveAttachedLights(pEntity);	
							pEntity->DeleteDrawable();
						}
					}

					if (pEntity->GetIsAnyManagedProcFlagSet()){
						OUTPUT_ONLY( if(pEntity->GetIsTypePed() && static_cast<CPed*>(pEntity)->GetNetworkObject()) Displayf("[interior] ped %s evict into world, reason=\"GetIsAnyManagedProcFlagSet\".", static_cast<CPed*>(pEntity)->GetNetworkObject()->GetLogName()); )

						// don't delete procedural objects (cleaned up by parent)
						CGameWorld::Remove(pEntity, true);
						CGameWorld::Add(pEntity, CGameWorld::OUTSIDE, true);			// evict into world
					} else {
						if (!pEntity->GetIsDynamic() || pEntity->GetIsTypeAnimatedBuilding()){
							CGameWorld::Remove(pEntity);	
							delete pEntity;				// safe to destroy
						} else {
							// dynamic entities can pose some extra problems...
							CDynamicEntity* pDE = static_cast<CDynamicEntity*>(pEntity);

							if (GetProxy()->ShouldEntityFreezeAndRetain(pDE)){
								GetProxy()->MoveToRetainList(pEntity);
							} else {
								if (pDE->GetIsTypeObject()){
									CObject* pObject = static_cast<CObject*>(pDE);
									if (pObject->GetRelatedDummy())
									{
										CObjectPopulation::ConvertToDummyObject( pObject, true );
										resetLoop = true;
									} else {
										if (pObject->CanBeDeleted()){
											CObjectPopulation::DestroyObject(pObject);
										}  else {
											OUTPUT_ONLY( if(pEntity->GetIsTypePed() && static_cast<CPed*>(pEntity)->GetNetworkObject()) Displayf("[interior] ped %s evict into world, reason=\"restarting the game with a player in the interior\".", static_cast<CPed*>(pEntity)->GetNetworkObject()->GetLogName()); )

										#if GTA_REPLAY
											fwInteriorLocation location = pObject->GetInteriorLocation();
										#endif // GTA_REPLAY

											// couldnt neither refreeze or destroy the object (can happen if restarting the game with a player in the interior)
											CGameWorld::Remove(pObject, true);
											pObject->GetPortalTracker()->DontRequestScaneNextUpdate();
											pObject->GetPortalTracker()->StopScanUntilProbeTrue();
										#if GTA_REPLAY
											if(pObject->GetOwnedBy() == ENTITY_OWNEDBY_REPLAY)
											{
												if(CReplayMgr::NotifyInteriorToExteriorEviction(pObject, location) == false) // Let Replay place object back into the world.
													CGameWorld::Add(pObject, CGameWorld::OUTSIDE, true); // Not yet handled by Replay - evict into world as normal.
											}
											else
										#endif // GTA_REPLAY
												CGameWorld::Add(pObject, CGameWorld::OUTSIDE, true); // evict into world
										}
									}
								} else {
									if (pDE->GetIsTypePed() && pDE->PopTypeIsRandomNonPermanent() && !pDE->IsNetworkClone() REPLAY_ONLY(&& CReplayMgr::IsEditModeActive() == false))
									{
										OUTPUT_ONLY( if(pDE->GetIsTypePed() && static_cast<CPed*>(pDE)->GetNetworkObject()) Displayf("[interior] ped %s detroyed, reason=\"destroy ambient peds when their interior is streamed out\".", static_cast<CPed*>(pDE)->GetNetworkObject()->GetLogName()); )

										// B* 486319 - destroy ambient peds when their interior is streamed out.
										CPed* pAmbientPed = static_cast<CPed*>(pDE);
										CPedFactory::GetFactory()->DestroyPed(pAmbientPed);
									}
									else
									{
										OUTPUT_ONLY( if(pDE->GetIsTypePed() && static_cast<CPed*>(pDE)->GetNetworkObject()) Displayf("[interior] ped %s evict into world, reason=\"could neither refreeze or destroy\".", static_cast<CPed*>(pDE)->GetNetworkObject()->GetLogName()); )

										// could neither refreeze or destroy the dynamic entity - dumped into world 
										CGameWorld::Remove(pDE, true);
										pDE->GetPortalTracker()->DontRequestScaneNextUpdate();
										pDE->GetPortalTracker()->StopScanUntilProbeTrue();
										CGameWorld::Add(pDE, CGameWorld::OUTSIDE, true);			// evict into world
									}
								}
							}
						}
					}
				}
			}
		}

#if __BANK
		Assert( m_roomData[i].GetRoomSceneNode()->GetEntityCount() == 0 );
		u32 roomEntityCount = m_roomData[i].GetRoomSceneNode()->GetEntityCount();
		if (roomEntityCount != 0)
		{
			Assertf(false, "could not remove entities from %s : room %d", GetBaseModelInfo()->GetModelName(), i);
			atArray< fwEntity* >		entities;
			m_roomData[i].GetRoomSceneNode()->GetEntityContainer()->GetEntityArray( entities );
			for(u32 j = 0; j<roomEntityCount; j++)
			{
				CEntity* pEntity = static_cast< CEntity* >( entities[j] );
				CDebug::DumpEntity(pEntity);
			}
		}
#endif //__BANK
	} 

	PortalObjectsDestroy();

	Assert(CPed::IsAnyPedInInterior(this) == false);

	m_pInternalLOD = NULL;

	m_bIsPopulated = false;

	// ----

	// remove & delete all the portal objects in the world created for this interior
	CPortalInst* pPortalInst = NULL;
	for(s32 i = 0; i < m_aRoomZeroPortalInsts.GetCount(); i++){
		pPortalInst =  m_aRoomZeroPortalInsts[i];
		if (pPortalInst){
			// don't delete portals which link interiors - try to promote the linked interior to owner instead
			if (this == pPortalInst->GetPrimaryInterior()){
				CInteriorInst*	pLinkedInt = pPortalInst->GetLinkedInterior();
				if ( pLinkedInt )
				{
					u32			globalPortalIndex = pLinkedInt->GetPortalIdxInRoom( 0, pPortalInst->GetLinkedPortalIndex() );
					pLinkedInt->m_aPortals[ globalPortalIndex ]->SetPositivePortalEnd( NULL );

					pPortalInst->PromoteLinkedInterior();
					continue;	// don't delete the portal!
				}
			}

			// don't delete portals which link interiors - remove information about the link instead
			if (this == pPortalInst->GetLinkedInterior()){
				CInteriorInst*	pPrimaryInt = pPortalInst->GetPrimaryInterior();
				if ( pPrimaryInt )
				{
					u32			globalPortalIndex = pPrimaryInt->GetPortalIdxInRoom( 0, pPortalInst->GetPrimaryPortalIndex() );
					pPrimaryInt->m_aPortals[ globalPortalIndex ]->SetPositivePortalEnd( NULL );

					pPortalInst->ClearLinkedInterior();
					continue;	// don't delete the portal!
				}
			}

			// portal was not linked to another interior so safe to delete
			CGameWorld::Remove( pPortalInst );
			delete(pPortalInst);
		}
	}

	LightEntityMgr::Update();

	m_bInterfacePortalInstsCreated = false;

	m_aRoomZeroPortalInsts.Reset();

	// Remove all the interior's timecycle modifiers.
	g_timeCycle.RemoveModifierBoxes(m_pProxy->GetInteriorProxyPoolIdx(),true);

	ShutdownSubSceneGraph();
	m_bArePortalsCreated = false;

	ShutdownInstanceData();

	Assert( !m_bIsPopulated);
	Assert( m_aRoomZeroPortalInsts.GetCount() == 0);

	CPortal::ms_interiorPopulationScanCode++;

	visibilityDebugf1("CPortal::ms_interiorPopulationScanCode incremented to %d by CInteriorInst::DepopulateInteriorContents", CPortal::ms_interiorPopulationScanCode);

	// Remove physics
	if (GetCurrentPhysicsInst())
	{
		RemovePhysics();
		DeleteInst();
	}

	m_bIsShellPopulated = false;
	m_bIsContentsPopulated = false;

	return(true);
}

// scheduled update of interior inst (go over all interiors over a period of N frames)
void CInteriorInst::Update(void)
{

	for(s32 i=0;i<m_roomData.GetCount();i++) {
#if __DEV
		fwPtrNode* pNode = m_roomData[i].m_activeTrackerList.GetHeadPtr();
		while(pNode != NULL){
			CPortalTracker* pPT =  static_cast<CPortalTracker*>(pNode->GetPtr());
			Assert(pPT);
			Assert(pPT->GetInteriorInst() == this);
			pNode = pNode->GetNextPtr();
		}
#endif //__DEV

		for (s32 j=0; j<MAX_SMOKE_LEVEL_SECTIONS; j++)
		{
			for (s32 k=0; k<MAX_SMOKE_LEVEL_SECTIONS; k++)
			{
				m_roomData[i].m_smokeLevel[j][k] = MAX(0.0f, m_roomData[i].m_smokeLevel[j][k]-VFXWEAPON_INTSMOKE_DEC_PER_UPDATE);
			}
		}
	}

	// each interior stores distance to any exterior portals it may have instanced
	Vector3 camPos = camInterface::GetPos();
	m_closestExternPortalDist = 1000.0f;	
	u32 numPortals = GetNumPortalsInRoom(0);

	if (!m_bIsPopulated){
		m_closestExternPortalDist = 1000.0f;
	} else {
		for(u32 i=0;i<numPortals;i++){

			u32	globalPortalIdx = 0;
			globalPortalIdx = GetPortalIdxInRoom(0,i);
			fwPortalCorners pc = GetPortal(globalPortalIdx);

			float dist = 1000.0f;
			pc.CalcCloseness(camPos, dist);

			if (dist < m_closestExternPortalDist){
				m_closestExternPortalDist = MAX(dist, 0.0f);
			}
		}
	}

	// if the interior now has a valid physics instance in the world, then it will attempt to capture peds & vehicles
	// in the world which should be added to the interior (if it has not already done so)
	bool bCollisionsActive = (GetCurrentPhysicsInst() || GetProxy()->AreStaticCollisionsLoaded());
	if (!m_bHasCapturedFlag && (GetProxy()->GetCurrentState() >= CInteriorProxy::PS_FULL) && bCollisionsActive){
		CapturePedsAndVehicles();

		spdSphere testSphere(CEntity::GetBoundSphere());
		CPortalVisTracker::RequestResetRenderPhaseTrackers(&testSphere);
	}

	// flush anything in the retain array into the interior, if the interior is ready
	if ((GetProxy()->GetCurrentState() >= CInteriorProxy::PS_FULL) &&  bCollisionsActive && (GetRetainList().CountElements() > 0)){
		GetProxy()->ExtractRetainedEntities();
	}

	// ensure the internal LOD is always created - to avoid light glitching
	if (m_pInternalLOD)
	{
		if (!m_pInternalLOD->GetDrawHandlerPtr())
		{
			m_pInternalLOD->CreateDrawable();
		}
	}
}

void PedVehicleAndObjectCapture(fwPtrList& ptrList, CInteriorInst* pIntInst)
{
	if (!AssertVerify(pIntInst!=NULL)){
		return;
	}

	CEntity* pNextEntity=NULL;
	fwPtrNode* pNode = ptrList.GetHeadPtr();

	if(pNode)
		pNextEntity = (CEntity*) pNode->GetPtr();

	while (pNode != NULL)
	{
		CEntity* pEntity = pNextEntity;
		pNode = pNode->GetNextPtr();
		if(pNode)
		{
			pNextEntity = (CEntity*) pNode->GetPtr();
			PrefetchDC(pNextEntity);
		}

		// trigger rescan for vehicles, peds, pickups and script created objects
		if (pEntity && pEntity->GetIsDynamic())
		{
			CDynamicEntity* pDE = static_cast<CDynamicEntity*>(pEntity);
			CPortalTracker* pPT = pDE->GetPortalTracker();
			Assert(pPT);

			bool bTriggerRescan = false;

			if (pDE->GetIsTypeVehicle() || pDE->GetIsTypePed())
			{
				bTriggerRescan = true;
			} 
			else if (pEntity->GetIsTypeObject())
			{
				CObject* pObject = static_cast<CObject*>(pEntity);
				if (pObject->IsPickup() || pObject->GetAsProjectile() || (pObject->GetOwnedBy() == ENTITY_OWNEDBY_SCRIPT))
				{
					bTriggerRescan = true;
				}
			}

			if (bTriggerRescan)
			{
				if (BANK_ONLY(gUsePortalTrackerCapture ||) !__BANK)
				{
					pPT->RequestCaptureForInterior();
				}
				else
				{
					pPT->RequestRescanNextUpdate();
				}
				pPT->Update(VEC3V_TO_VECTOR3(pDE->GetTransform().GetPosition()));
			}
		}
	}
}

// attempt to capture peds and vehicles which are within close contact of collision mesh of this interior
void CInteriorInst::CapturePedsAndVehicles(void){

	fwPtrListSingleLink	entityList;

	if (GetCurrentPhysicsInst() || GetProxy()->AreStaticCollisionsLoaded()){
		m_bHasCapturedFlag = true;

		fwIsSphereIntersecting testSphere(CEntity::GetBoundSphere());
		CGameWorld::ForAllEntitiesIntersecting(&testSphere, gWorldMgr.AppendToLinkedListCB, (void*) &entityList, (ENTITY_TYPE_MASK_VEHICLE|ENTITY_TYPE_MASK_PED|ENTITY_TYPE_MASK_OBJECT), SEARCH_LOCATION_EXTERIORS, SEARCH_LODTYPE_HIGHDETAIL, SEARCH_OPTION_NONE, WORLDREP_SEARCHMODULE_PORTALS);
	
		PedVehicleAndObjectCapture(entityList, this);
	}
}


// remove all the instanced portals out of the world
void CInteriorInst::ShutdownLevel(void)
{
	CInteriorInst::HoldRoomZeroModels(NULL);
}

// when linking portals need the corners to be accurately aligned 
void CInteriorInst::MergePortalCorners(s32 srcPortalIdx, CPortalInst* pPortalInst){

	// src portal to take the points from
	fwPortalCorners	cornerPts;
	GetPortalInRoom(0,srcPortalIdx,cornerPts);

	CInteriorInst* pDestIntInst = NULL;
	s32	destPortalIdx = -1;
	pPortalInst->GetOwnerData(pDestIntInst, destPortalIdx);

	// get the idx of the portal in the storage array from it's (room,portal) pair
	u32 portalId = pDestIntInst->GetPortalIdxInRoom(0, destPortalIdx);

	fwPortalCorners& destPortalCorners = pDestIntInst->GetPortal(portalId);

#if __ASSERT
	bool bPointUsed[4] = {false,false,false,false};
#endif

	for(u32 i=0;i<4;i++){
		for(u32 j=0;j<4;j++){

			Vector3 diff = (destPortalCorners.GetCorner(i) - cornerPts.GetCorner(j));

			// if less than 75cm apart then merge
			if (diff.Mag2() < 0.5625f){
				Assert(bPointUsed[j] == false);
				destPortalCorners.SetCorner( i, cornerPts.GetCorner(j) );
				ASSERT_ONLY(bPointUsed[j] = true;)
				break;
			}
		}
	}

	// check for unmerged points
	for(u32 i=0;i<4;i++){
		Assertf(bPointUsed[i], "[Art] Portal merge failed @ (%.2f,%.2f,%.2f)\n", cornerPts.GetCorner(i).x,
			cornerPts.GetCorner(i).y, cornerPts.GetCorner(i).z);
	}
}

// if the given point is very close (15cm) to the surface of the given portal, then
// return which room the point belongs in
s32	CInteriorInst::TestPointAgainstPortal(s32 portalIdx, Vector3::Vector3Param pos) const {

	CMloModelInfo *pModelInfo = GetMloModelInfo();
	Assert(pModelInfo);

	sceneAssert(pModelInfo->GetIsStreamType());
	Assert(portalIdx < (s32)GetNumPortals());

	u32	room1Idx,room2Idx;
	CPortalFlags dontCare;
	pModelInfo->GetPortalData(portalIdx, room1Idx, room2Idx, dontCare);

	const fwPortalCorners& destPortalCorners = GetPortal(portalIdx);
	float distToPortal = 1000.0f;
	s32 destRoom = -1;
	float fOffset = 0.25f;

#if GTA_REPLAY
	//Increase the offset for replay free camera
	if(CReplayMgr::IsEditModeActive())
	{
		const camBaseCamera* renderedCamera = camInterface::GetDominantRenderedCamera();
		if(renderedCamera && renderedCamera->GetIsClassId(camReplayFreeCamera::GetStaticClassId()))
		{
			fOffset = 0.35f;
		}
	}
#endif //GTA_REPLAY
	// within 25 cm on each side
	if (destPortalCorners.CalcCloseness(pos, distToPortal)){
		// determine which side of the portal we are on
		if (distToPortal < 0.0f && distToPortal > -fOffset){
			destRoom = room1Idx;
		} else if (distToPortal > 0.0f && distToPortal < fOffset){
			destRoom = room2Idx;
		}
	}
	return(destRoom);
}

bool CInteriorInst::SetInstanceTransform(Vector3& trans, Quaternion& orien)
{
	// - Careful : at this point we are going to compute a full matrix transform for the interior using the given quaternion
	// THIS IS NOT THE SAME as what happens in fwEntity::InitTransformFromDefinition() - that function alters the quaternion first!

	// --- this interior has a simple orientation - we can avoid using full matrices for objects inside it
	const fwTransform*	pTransform = GetTransformPtr();
	m_bNeedsFullTransform = !(pTransform->IsSimpleTransform());

	// ---- set up position and orientation
 	QuatV q(orien.x, orien.y, orien.z, orien.w);
 	Vec3V pos = RCC_VEC3V(trans);
 	Mat34V m;
 	Mat34VFromQuatV(m, q, pos);
	#if ENABLE_MATRIX_MEMBER		
	SetTransform(m);
	SetTransformScale(1.0f, 1.0f);
	#else
	SetTransform(rage_new fwMatrixTransform(m));
	#endif
	m_numExitPortals = GetProxy()->GetNumExitPortals();

	return(true);
}

void CInteriorInst::GenerateInstanceData()
{
	sceneAssert( !m_bInstanceDataGenerated );

	CMloModelInfo *pModelInfo = GetMloModelInfo();
	sceneAssert(pModelInfo);
	sceneAssert(pModelInfo->GetIsStreamType());

	// allocate space for object instance ptrs of objects which populate each room
	u32 numRooms = pModelInfo->GetRooms().GetCount();
	m_roomData.Reset();
	m_roomData.Resize(numRooms);

	m_bContainsWater = false;

//	u32 baseIdxForRoom = 0;
	for(u32 i=0;i<numRooms;i++) 
	{
		m_roomData[i].m_activeTrackerList.Flush();

		// reset data about water table for each room
		m_roomData[i].m_localWaterLevel = 0.0f;
		m_roomData[i].m_bHasLocalWaterLevel = false;

		// init the smoke level
		for (s32 j=0; j<MAX_SMOKE_LEVEL_SECTIONS; j++)
		{
			for (s32 k=0; k<MAX_SMOKE_LEVEL_SECTIONS; k++)
			{
				m_roomData[i].m_smokeLevel[j][k] = 0.0f;
			}
		}

		// calc the smoke grid size for this room
		Vector3 bbMin = GetMloModelInfo()->GetBoundingBoxMin();
		Vector3 bbMax = GetMloModelInfo()->GetBoundingBoxMax();

		float width = bbMax.x - bbMin.x;
		float depth = bbMax.y - bbMin.y;

		float gridSizeX = width/MAX_SMOKE_LEVEL_SECTIONS;
		float gridSizeY = depth/MAX_SMOKE_LEVEL_SECTIONS;

		gridSizeX = MAX(gridSizeX, VFXWEAPON_INTSMOKE_MIN_GRID_SIZE);
		gridSizeY = MAX(gridSizeY, VFXWEAPON_INTSMOKE_MIN_GRID_SIZE);

		m_roomData[i].m_smokeGridSize = MAX(gridSizeX, gridSizeY);

		atHashWithStringNotFinal timeCycleName;
		atHashWithStringNotFinal secondaryTimeCycleName;
		pModelInfo->GetRoomTimeCycleNames(i, timeCycleName, secondaryTimeCycleName);
		if(timeCycleName.IsNotNull())
		{
			m_roomData[i].m_timeCycleIndex = g_timeCycle.FindModifierIndex(timeCycleName, "AmbScale");
			Assert(m_roomData[i].m_timeCycleIndex >= 0);
		}
		else
		{
			m_roomData[i].m_timeCycleIndex = -1;
		}
		if(secondaryTimeCycleName.IsNotNull())
		{
			m_roomData[i].m_timeCycleIndexSecondary = g_timeCycle.FindModifierIndex(secondaryTimeCycleName, "AmbScale");
			Assert(m_roomData[i].m_timeCycleIndexSecondary >= 0);
		}
		else
		{
			m_roomData[i].m_timeCycleIndexSecondary = -1;
		}

		int numPortals = GetNumPortalsInRoom(i);
		m_roomData[i].m_portalIndices.Reset();
		m_roomData[i].m_portalIndices.Resize(numPortals);
		for(int iPortal = 0; iPortal < numPortals; iPortal++)
		{
			m_roomData[i].m_portalIndices[iPortal] = FindGlobalPortalIdx(pModelInfo, i, iPortal);
		}
	}

	m_bInstanceDataGenerated = true;
}

void CInteriorInst::ShutdownInstanceData()
{
	Assert( m_bInstanceDataGenerated );

	for(int i = 0; i < m_roomData.GetCount(); i++)
	{
		m_roomData[i].m_portalIndices.Reset();
	}

	m_roomData.Reset();

	m_bInstanceDataGenerated = false;
}

// Appends the given object ptr to the object list array for this interior. Array will grow if necessary.
// roomIdx specifies which room it is to be added to.
void CInteriorInst::AppendObjectInstance(CEntity* pEntity, fwInteriorLocation interiorLocation){

	(void) interiorLocation;

#if __ASSERT
	Assert( m_bInstanceDataGenerated );

	Assert(pEntity);
	Assert(pEntity->GetIsTypeObject() == false);				// objects have portal trackers and go on active object list for room

	// dummy objects from map _must not_ go into the interior
	if (pEntity->GetIsTypeDummyObject()){
		Assert(pEntity->GetIplIndex() == 0);
	}

	CMloModelInfo *pModelInfo = GetMloModelInfo();
	Assert(pModelInfo);

#endif

	if ( interiorLocation.IsAttachedToRoom() )
	{
		const s32		roomIdx = interiorLocation.GetRoomIndex();

		// if this model defines water level for MLOs then make sure the water level for this room is set from it.
		CBaseModelInfo* pEntityModelInfo = pEntity->GetBaseModelInfo();
		Assert(pEntityModelInfo);
		if ((roomIdx < m_roomData.GetCount()) && (pEntityModelInfo->TestAttribute(MODEL_ATTRIBUTE_MLO_WATER_LEVEL))){
			m_roomData[roomIdx].m_localWaterLevel = pEntity->GetTransform().GetPosition().GetZf();
			m_roomData[roomIdx].m_bHasLocalWaterLevel = true;
			m_bContainsWater = true;
		}
	}

	Assert(pEntity);

#if __DEV
	// dummies can't be appended to interiors! (not valid in room 0)
	if (pEntity->GetIsTypeDummyObject() && interiorLocation.IsAttachedToRoom() && interiorLocation.GetRoomIndex() == 0){
		CBaseModelInfo* pEntityModelInfo = pEntity->GetBaseModelInfo();
		Vector3 pos;
		GetBoundCentre(pos);
		const Vector3 vThisPosition = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
		artAssertf(false,"Dummy for dynamic entity %s can't be added to room 0 (limbo) in %s at (%d,%d,%d). "
			"Dynamic entities must be assigned to a room other than the limbo for portal tracking purposes.",
			pEntityModelInfo->GetModelName(), pModelInfo->GetModelName(), (int)vThisPosition.x,(int)vThisPosition.y,(int)vThisPosition.z);
		return;
	}
#endif //__DEV

	if ( interiorLocation.IsAttachedToRoom() ){
		
		const s32		roomIdx = interiorLocation.GetRoomIndex();

		Assert (roomIdx < m_roomData.GetCount());			//check if room we want to add to is valid

		// add this entity to the entity array for this room
		pEntity->m_nFlags.bInMloRoom = true;
		m_roomData[roomIdx].GetRoomSceneNode()->GetEntityContainer()->AppendEntity( pEntity );

	} else if ( interiorLocation.IsAttachedToPortal() )
	{
		const s32		portalIdx = interiorLocation.GetPortalIndex();

		Assert(portalIdx < (s32)GetNumPortals());
		//Assertf(!pEntity->GetIsTypeLight(), "interior %s : lights not allowed to be attached to portals",GetProxy()->GetModelName());	// not expecting to see these attached to portals.
#if __ASSERT
		if (m_aPortals[portalIdx]->GetIsLinkPortal())
		{
			CBaseModelInfo *pModelInfo = pEntity->GetBaseModelInfo();

			if (pModelInfo->IsStreamedArchetype())
			{
				if (!fwArchetypeManager::IsArchetypePermanentDLC(pModelInfo))
				{
					Assertf(false, "Link portals cannot have dependency streamed archetypes in them (crashy!), model name %s, interior name %s", pModelInfo->GetModelName(), GetBaseModelInfo()->GetModelName());
				}
			}
		}
#endif
		m_aPortals[portalIdx]->GetEntityContainer()->AppendEntity(pEntity);
		pEntity->m_nFlags.bInMloRoom = true;
	} else {
		Assert(false);
	}
}

// remove this object from the room object list array. 
void CInteriorInst::RemoveObjectInstance(CEntity* pEntity){

	Assert( m_bInstanceDataGenerated );
	Assert(this == CInteriorProxy::GetFromLocation(pEntity->GetInteriorLocation())->GetInteriorInst());

	if (pEntity->GetIsRetainedByInteriorProxy()){
		GetProxy()->RemoveFromRetainList(pEntity);
	}

	if ( pEntity->GetInteriorLocation().IsAttachedToRoom() )
	{
		const s32		roomIndex = pEntity->GetInteriorLocation().GetRoomIndex();
		sceneAssert ( roomIndex >= 0 && roomIndex < (s32) GetNumRooms() );

		m_roomData[ roomIndex ].GetRoomSceneNode()->GetEntityContainer()->RemoveEntity(pEntity);
		pEntity->m_nFlags.bInMloRoom = false;
	}
	else if ( pEntity->GetInteriorLocation().IsAttachedToPortal() )
	{
		const s32		portalIndex = pEntity->GetInteriorLocation().GetPortalIndex();
		sceneAssert( portalIndex >= 0 && portalIndex < (s32) GetNumPortals() );

		m_aPortals[ portalIndex ]->GetEntityContainer()->RemoveEntity(pEntity);
		pEntity->m_nFlags.bInMloRoom = false;
	}
	else
		sceneAssertf( false, "Entity is inside interior but neither in a room or portal" );
}

// update the object in this interior
void CInteriorInst::UpdateObjectInstance(CEntity* pEntity){

	sceneAssert( m_bInstanceDataGenerated );
	sceneAssert(this == CInteriorProxy::GetFromLocation(pEntity->GetInteriorLocation())->GetInteriorInst());

	if ( pEntity->GetInteriorLocation().IsAttachedToRoom() )
		m_roomData[ pEntity->GetInteriorLocation().GetRoomIndex() ].GetRoomSceneNode()->GetEntityContainer()->UpdateEntityDescriptorInNode( pEntity );
	else if ( pEntity->GetInteriorLocation().IsAttachedToPortal() )
		m_aPortals[ pEntity->GetInteriorLocation().GetPortalIndex() ]->GetEntityContainer()->UpdateEntityDescriptorInNode( pEntity );
	else
		sceneAssertf( false, "Entity is inside interior but neither in a room or portal" );
}

// add this object to the active tracker list, set tracker data from data in the entity
void CInteriorInst::AddObjectInstance(CDynamicEntity* pEntity, fwInteriorLocation interiorLocation) 
{
	sceneAssertf( m_bInstanceDataGenerated, "interior: %s not yet capable of receiving object (%s)!", GetMloModelInfo()->GetModelName(), pEntity->GetModelName() );

	sceneAssert(pEntity);
	if (!pEntity)
		return; //safety

	CPortalTracker* pPT = pEntity->GetPortalTracker();
	sceneAssert(pPT);
	if (!pPT)
		return; //safety

	// check that this object isn't in an interior
	sceneAssertf(!pPT->IsInsideInterior(), "Entity already in interior! Dumping...");

#if __DEV
	if (pPT->IsInsideInterior())
	{
		CDebug::DumpEntity(pEntity);
	}
#endif //__DEV

	// add the tracker for this object into the correct active list

	// is this object to be added to a room?
	if ( interiorLocation.IsAttachedToRoom())
	{
		const s32		roomIdx = interiorLocation.GetRoomIndex();
		sceneAssert( roomIdx >= 0 && roomIdx < (s32) GetNumRooms() );
		if ( roomIdx >= 0 && roomIdx < (s32) GetNumRooms() )
		{
			sceneAssert( m_roomData[roomIdx].GetRoomSceneNode() && m_roomData[roomIdx].GetRoomSceneNode()->GetEntityContainer() );
			if ( m_roomData[roomIdx].GetRoomSceneNode() && m_roomData[roomIdx].GetRoomSceneNode()->GetEntityContainer() )
			{
				pPT->SetInteriorAndRoom(this, roomIdx);
				m_roomData[roomIdx].GetRoomSceneNode()->GetEntityContainer()->AppendEntity(pEntity);
				pEntity->m_nFlags.bInMloRoom = true;

				// if owned by script, ensure we add any necessary lights back into interior (restricted to just the arcade interior for now!)
				if (GetProxy() && GetProxy()->GetIsRegenLightsOnAdd())
				{
					if (pEntity->GetOwnedBy() == ENTITY_OWNEDBY_SCRIPT)
					{
						CBaseModelInfo* pMI = pEntity->GetBaseModelInfo();
						if (pMI->Has2dEffects() && pEntity->m_nFlags.bLightObject == false)
						{
							pEntity->m_nFlags.bLightObject = LightEntityMgr::AddAttachedLights(pEntity);
						}
					}
				}
			}
		}
	} 
	else if ( interiorLocation.IsAttachedToPortal() )
	{
		// is this object to be added to a portal?
		const s32		portalIdx = interiorLocation.GetPortalIndex();
		sceneAssert( portalIdx >= 0 && portalIdx < (s32) GetNumPortals() );

		if ( portalIdx >= 0 && portalIdx < (s32) GetNumPortals() )
		{
			sceneAssert( m_aPortals[portalIdx] && m_aPortals[portalIdx]->GetEntityContainer() );
			if ( m_aPortals[portalIdx] && m_aPortals[portalIdx]->GetEntityContainer() )
			{
				pPT->SetDontProcessUpdates(true);			// don't track moving through interior

				//Assert(!pEntity->GetIsTypeLight());		// not expecting to see these attached to portals.

				m_aPortals[portalIdx]->GetEntityContainer()->AppendEntity(pEntity);
				pEntity->m_nFlags.bInMloRoom = true;
			}
		}

	} 
	else 
	{
		sceneAssertf(false, "Invalid initialization idx for dynamic entity");
	}

	AddToActiveTrackerList(pPT);

	// check portal tracker / entity are consistent with each other
	sceneAssert(pPT->GetOwner() == pEntity);
}

// remove this object from the active tracker list
void CInteriorInst::RemoveObjectInstance(CDynamicEntity* pEntity){

	sceneAssert( m_bInstanceDataGenerated );

	// check that this object is actually in this interior
	sceneAssert(this == CInteriorProxy::GetFromLocation(pEntity->GetInteriorLocation())->GetInteriorInst());

	// must be on the active list - so remove it
	CPortalTracker* pPT = pEntity->GetPortalTracker();
	sceneAssert(pPT);
	RemoveFromActiveTrackerList(pPT);

	if (pEntity->GetIsRetainedByInteriorProxy()){
		GetProxy()->RemoveFromRetainList(pEntity);
	}

	// if the object was attached to a portal, the ref needs cleared.
	if (pEntity->GetInteriorLocation().IsAttachedToRoom()){

		const s32		roomIndex = pEntity->GetInteriorLocation().GetRoomIndex();

		m_roomData[roomIndex].GetRoomSceneNode()->GetEntityContainer()->RemoveEntity(pEntity);
		pEntity->m_nFlags.bInMloRoom = false;
	}
	else if (pEntity->GetInteriorLocation().IsAttachedToPortal()){

		const s32		portalIndex = pEntity->GetInteriorLocation().GetPortalIndex();

		m_aPortals[portalIndex]->GetEntityContainer()->RemoveEntity(pEntity);
		pEntity->m_nFlags.bInMloRoom = false;
	}
	else
		sceneAssertf( false, "Entity is in neither a room or portal" );
}

Vector3	CInteriorInst::GetBoundCentre() const
{
	return(m_boundSphereCentre);
}

void CInteriorInst::GetBoundCentre(Vector3& ret) const
{
	ret = m_boundSphereCentre;
}

float CInteriorInst::GetBoundCentreAndRadius(Vector3& centre) const
{
	centre = m_boundSphereCentre;
	return m_boundSphereRadius;
}

void CInteriorInst::GetBoundSphere(spdSphere& sphere) const
{
	sphere.Set(RCC_VEC3V(m_boundSphereCentre), ScalarV(m_boundSphereRadius));
}

const Vector3& CInteriorInst::GetBoundingBoxMin() const
{
	return(m_boundBox.GetMinVector3());
}

const Vector3& CInteriorInst::GetBoundingBoxMax() const
{
	return(m_boundBox.GetMaxVector3());
}


FASTRETURNCHECK(const spdAABB &) CInteriorInst::GetBoundBox(spdAABB& /*box*/) const
{
	return m_boundBox;
}

bool	CInteriorInst::IsSubwayMLO(void) const
{
	CMloModelInfo *pModelInfo = GetMloModelInfo();
	Assert(pModelInfo);

	return(((pModelInfo->GetMLOFlags() & INTINFO_MLO_SUBWAY) == INTINFO_MLO_SUBWAY));
}

bool	CInteriorInst::IsOfficeMLO(void) const
{
 	CMloModelInfo *pModelInfo = GetMloModelInfo();
 	Assert(pModelInfo);

	return(((pModelInfo->GetMLOFlags() & INTINFO_MLO_OFFICE) == INTINFO_MLO_OFFICE));
}

bool	CInteriorInst::IsAllowRunMLO(void) const
{
	CMloModelInfo *pModelInfo = GetMloModelInfo();
	Assert(pModelInfo);

	return(((pModelInfo->GetMLOFlags() & INTINFO_MLO_ALLOWRUN) == INTINFO_MLO_ALLOWRUN)); 	
}

bool	CInteriorInst::IsNoWaterReflection(void) const
{
	CMloModelInfo *pModelInfo = GetMloModelInfo();
	Assert(pModelInfo);

	return(((pModelInfo->GetMLOFlags() & INTINFO_MLO_NO_WATER_REFLECTION) == INTINFO_MLO_NO_WATER_REFLECTION)); 	
}

u32	CInteriorInst::GetNumPortals(void) const{
	CMloModelInfo *pModelInfo = GetMloModelInfo();
	sceneAssert(pModelInfo);
	sceneAssert(pModelInfo->GetIsStreamType());

	return(pModelInfo->GetPortals().GetCount());
}

u32	CInteriorInst::GetNumRooms(void) const{
	CMloModelInfo *pModelInfo = GetMloModelInfo();
	sceneAssert(pModelInfo);
	sceneAssert(pModelInfo->GetIsStreamType());

	return(pModelInfo->GetRooms().GetCount());
}

u32	CInteriorInst::GetNumPortalsInRoom(u32 roomIdx) const{

	CMloModelInfo *pModelInfo = GetMloModelInfo();
	Assert(pModelInfo);

	sceneAssert(pModelInfo->GetIsStreamType());
	sceneAssert(roomIdx < pModelInfo->GetRooms().GetCount());

	return(pModelInfo->GetRooms()[roomIdx].m_portalCount);
}

void	CInteriorInst::CalcTransformedBounds()
{
	// get the initial data bounding sphere & transform to current position
	spdSphere proxySphere;
	GetProxy()->GetBoundSphere(proxySphere);
	m_boundSphereCentre = VEC3V_TO_VECTOR3(proxySphere.GetCenter());
	m_boundSphereRadius = proxySphere.GetRadius().Getf();;
	
	m_boundBox.Invalidate();

	// make sure bound box is big enough to hold bounding sphere
	Vector3		boundBoxCorner;
	boundBoxCorner = m_boundSphereCentre;
	boundBoxCorner.x += m_boundSphereRadius;
	boundBoxCorner.y += m_boundSphereRadius;
	m_boundBox.GrowPoint(RCC_VEC3V(boundBoxCorner));

	boundBoxCorner.x -= (m_boundSphereRadius*2.0f);		// x2 because we are re-using boundBoxCorner!
	boundBoxCorner.y -= (m_boundSphereRadius*2.0f);
	m_boundBox.GrowPoint(RCC_VEC3V(boundBoxCorner));
}

// return global portal index given a room index and a room relative portal index
s32	CInteriorInst::GetPortalIdxInRoom(u32 roomIdx, u32 roomPortalIdx) const {
	
	if(m_bInstanceDataGenerated)
	{
		return m_roomData[roomIdx].m_portalIndices[roomPortalIdx];
	}

	CMloModelInfo *pModelInfo = GetMloModelInfo();
	sceneAssert(pModelInfo);
	sceneAssert(pModelInfo->GetIsStreamType());
	return FindGlobalPortalIdx(pModelInfo, roomIdx, roomPortalIdx);
}

u32	CInteriorInst::GetNumObjInRoom(u32 roomIdx) const
{ 
	sceneAssert(roomIdx < GetNumRooms());		//check room idx is valid

	if ( !m_bIsPopulated )
		return 0;

	return(m_roomData[roomIdx].GetRoomSceneNode()->GetEntityCount());
}

s32	CInteriorInst::GetFloorIdInRoom(u32 roomIdx) const
{ 
	CMloModelInfo *pModelInfo = GetMloModelInfo();
	sceneAssert(pModelInfo);
	sceneAssert(pModelInfo->GetIsStreamType());

	const CMloRoomDef& room = pModelInfo->GetRooms()[roomIdx];
	return(room.m_floorId);
}

u32	CInteriorInst::GetNumObjInPortal(u32 portalIdx) const
{ 
	sceneAssert(portalIdx < GetNumPortals());

	if ( !m_bIsPopulated )
		return 0;

	sceneAssert( m_bInstanceDataGenerated );

	return(m_aPortals[portalIdx]->GetEntityCount());
}

u32	CInteriorInst::GetNumExitPortalsInRoom(u32 roomIdx) const 
{
	CMloModelInfo *pModelInfo = GetMloModelInfo();
	sceneAssert(pModelInfo);
	sceneAssert(pModelInfo->GetIsStreamType());
	
	s32 exteriorPortalCount = 0;
	for(u32 i=0; i<pModelInfo->GetPortals().GetCount(); i++)
	{
		const CMloPortalDef& portal = pModelInfo->GetPortals()[i];
		if (portal.m_roomFrom == roomIdx && portal.m_roomTo == 0)
		{
			exteriorPortalCount++;
		} else 	if (portal.m_roomFrom == 0 && portal.m_roomTo == roomIdx)
		{
			exteriorPortalCount++;
		}
	}

	return(exteriorPortalCount);
}

CEntity*	CInteriorInst::GetObjInstancePtr(u32 roomIdx, u32 objIdx) const
{
	sceneAssert(m_bIsPopulated);
	sceneAssert(roomIdx < GetNumRooms());		//check room idx is valid
	sceneAssert(objIdx < GetNumObjInRoom(roomIdx));		//check object idx is valid for this room

	CEntity* pEntity = NULL;
	pEntity = static_cast< CEntity* >( m_roomData[roomIdx].GetRoomSceneNode()->GetEntity(objIdx) );
	return(pEntity);
}

// the ptr to the instanced object attached to this portal index in this interior
CEntity*	CInteriorInst::GetPortalObjInstancePtr(s32 roomIdx, s32 portalIdx, u32 slotIdx) const
{
	sceneAssert(m_bIsShellPopulated);
	sceneAssert( m_bInstanceDataGenerated );
	sceneAssert(portalIdx < static_cast<s32>(GetNumPortals()));		//check portal idx is valid
	sceneAssert(roomIdx > -1);
	sceneAssert(portalIdx > -1);

	s32 globalPortalIdx = GetPortalIdxInRoom(roomIdx, portalIdx);

	Assert(slotIdx < m_aPortals[globalPortalIdx]->GetEntityCount());
	if ((globalPortalIdx < m_aPortals.GetCount()) && (m_aPortals[globalPortalIdx]->GetEntityCount() > slotIdx))
	{
		return static_cast< CEntity* >( m_aPortals[globalPortalIdx]->GetEntity(slotIdx) );
	} else
	{
		return NULL;
	}
}

// determine if the specified portal is a link portal or not
bool	CInteriorInst::IsLinkPortal(u32 roomIdx, u32 portalIdx) const
{
	CMloModelInfo *pModelInfo = GetMloModelInfo();
	sceneAssert(pModelInfo);
	sceneAssert(pModelInfo->GetIsStreamType());
	{
		u32	interiorPortalIdx = 0;
		interiorPortalIdx = GetPortalIdxInRoom(roomIdx,portalIdx);
		return((pModelInfo->GetPortals()[interiorPortalIdx].m_flags & INTINFO_PORTAL_LINK) != 0);
	} 
}

bool	CInteriorInst::IsMirrorPortal(u32 roomIdx, u32 portalIdx) const
{
	CMloModelInfo *pModelInfo = GetMloModelInfo();
	sceneAssert(pModelInfo);
	sceneAssert(pModelInfo->GetIsStreamType());

	u32	interiorPortalIdx = 0;
	interiorPortalIdx = GetPortalIdxInRoom(roomIdx,portalIdx);
	return((pModelInfo->GetPortals()[interiorPortalIdx].m_flags & INTINFO_PORTAL_MIRROR) != 0);
}

// get the name associated with this room
const char* CInteriorInst::GetRoomName(u32 roomIdx) const
{
	CMloModelInfo *pModelInfo = GetMloModelInfo();
	sceneAssert(pModelInfo);
	sceneAssert(pModelInfo->GetIsStreamType());

	const CMloRoomDef& room = pModelInfo->GetRooms()[roomIdx];
	return(room.m_name.c_str());
}

// get the hash code of name associated with this room
u32 CInteriorInst::GetRoomHashcode(u32 roomIdx) const
{
	CMloModelInfo *pModelInfo = GetMloModelInfo();
	sceneAssert(pModelInfo);
	sceneAssert(pModelInfo->GetIsStreamType());

	const CMloRoomDef& room = pModelInfo->GetRooms()[roomIdx];
	return(atHashString::ComputeHash(room.m_name));
}

s32 CInteriorInst::FindRoomByHashcode(u32 hashcode) const
{
	CMloModelInfo *pModelInfo = GetMloModelInfo();
	sceneAssert(pModelInfo);
	sceneAssert(pModelInfo->GetIsStreamType());

	const atArray<CMloRoomDef>& rooms = pModelInfo->GetRooms();
	for(u32 i=0; i<rooms.GetCount();i++)
	{
		if (atHashString::ComputeHash(rooms[i].m_name) == hashcode)
		{
			return(i);
		}
	}

	return(-1);
}

// return the room Idx which is on the other side of the given portal in the given room.
s32	CInteriorInst::GetDestThruPortalInRoom(u32 roomIdx, u32 portalIdx) const {

	CMloModelInfo *pModelInfo = GetMloModelInfo();
	sceneAssert(pModelInfo);
	sceneAssert(pModelInfo->GetIsStreamType());

	u32 interiorPortalIdx = GetPortalIdxInRoom(roomIdx, portalIdx);
	 const CMloPortalDef& portal = pModelInfo->GetPortals()[interiorPortalIdx];

	u32 destinationRoom = (portal.m_roomTo == roomIdx) ? portal.m_roomFrom : portal.m_roomTo;

	 return(destinationRoom);
}

// return the given portal from the given room - the portal corners are ordered so that normal
// points into the given room
void CInteriorInst::GetPortalInRoom(u32 roomIdx, u32 portalIdx, fwPortalCorners& portalCorners) const {

	CMloModelInfo *pModelInfo = GetMloModelInfo();
	sceneAssert(pModelInfo);

	// get the portal index in the global table of all portals
	u32	interiorPortalIdx = 0;
	interiorPortalIdx = GetPortalIdxInRoom(roomIdx,portalIdx);

	// grab the corners of the portal
	fwPortalCorners pc = GetPortal(interiorPortalIdx);

	// the default order of portal corners is to point into room2
	u32 room1Idx,room2Idx;

	sceneAssert(pModelInfo->GetIsStreamType());
	room1Idx = pModelInfo->GetPortals()[interiorPortalIdx].m_roomFrom;
	room2Idx = pModelInfo->GetPortals()[interiorPortalIdx].m_roomTo;

	if (roomIdx == room2Idx){
		portalCorners = pc;
	} else {
		// need to re-order corners so that normal points into other room
		portalCorners = pc;
		portalCorners.SetCorner( 2, pc.GetCorner(0) );
		portalCorners.SetCorner( 0, pc.GetCorner(2) );
	}
}

void	CInteriorInst::CalcRoomBoundBox(u32 roomIdx, spdAABB& bbox) const{

	bbox.Set(Vec3V(99999.0f, 99999.0f, 99999.0f), Vec3V(-99999.0f, -99999.0f, -99999.0f));

	u32 numObjs = GetNumObjInRoom(roomIdx);
	for(u32 i=0;i<numObjs;i++){
		CEntity* pEntity = GetObjInstancePtr(roomIdx, i);
		// Don't assert on this. The entity might have been a temporary object like a fragment
		//Assert(pEntity);
		if(!pEntity) continue;
		if(VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()).IsZero()) continue;

		bbox.GrowPoint(pEntity->GetTransform().GetPosition());
	}

	s32 numPortals = GetNumPortalsInRoom(roomIdx);

	for(s32 i = 0;i<numPortals;i++){
		fwPortalCorners inPortal;
		GetPortalInRoom(roomIdx, i, inPortal);

		bbox.GrowPoint(VECTOR3_TO_VEC3V(inPortal.GetCorner(0)));		
		bbox.GrowPoint(VECTOR3_TO_VEC3V(inPortal.GetCorner(2)));		
	}
}


void CInteriorInst::RemovePortalInstanceRef(CPortalInst* pPortalInst){

	for(s32 i = 0; i < m_aRoomZeroPortalInsts.GetCount(); i++){
		if (pPortalInst && (pPortalInst == m_aRoomZeroPortalInsts[i])){
			m_aRoomZeroPortalInsts[i] = NULL;
		}
	}
}

// cleanup last refs to the detail objects which are part of the interior so they can get all data out of memory
// ie. the txd for the interior
void	CInteriorInst::DeleteDetailDrawables(void){

	if (!m_bIsPopulated) { return; }

	CEntity* pEntity = NULL;

	RestartEntityInInteriorList();
	while(GetNextEntityInInteriorList(&pEntity))
	{
		if(pEntity)
		{
			Assert(this == CInteriorProxy::GetFromLocation(pEntity->GetInteriorLocation())->GetInteriorInst());

			if (pEntity->GetOwnedBy() == ENTITY_OWNEDBY_INTERIOR)
			{
				pEntity->DeleteDrawable();
			}
		}
	} 

	//	Copied from destructor
	for(s32 i=0;i<m_roomData.GetCount();i++) {
		fwPtrNode* pNode = m_roomData[i].m_activeTrackerList.GetHeadPtr();
		while(pNode != NULL){
			CPortalTracker* pPT =  static_cast<CPortalTracker*>(pNode->GetPtr());
			Assert(pPT);
			pNode = pNode->GetNextPtr();
			CEntity*	pTrackedEntity = pPT->GetOwner();

			if (pTrackedEntity){
#if __DEBUG
				CBaseModelInfo* pModelInfo = pTrackedEntity->GetBaseModelInfo();
				Assert(pModelInfo);
				Assertf(	pTrackedEntity->GetInteriorLocation().GetInteriorProxyIndex() == CInteriorProxy::GetPool()->GetJustIndex(GetProxy()),"model in wrong room at shutdown:%s",pModelInfo->GetModelName());
#endif //__DEBUG

				if (pTrackedEntity->GetOwnedBy() == ENTITY_OWNEDBY_INTERIOR){
					if (pTrackedEntity->GetIsTypeObject()){
						pTrackedEntity->DeleteDrawable();
					} 
					else if (pTrackedEntity->GetIsTypePed()){
						Assertf(false, "Info : Peds or dummy peds shouldn't have interior info idx set\n");
					} 
				}
			}
		}
	}
	//	End of copied from destructor
}

// add to the list of trackers active in this room
void	CInteriorInst::AddToActiveTrackerList(CPortalTracker* pPT){

	Assert(pPT);
	s32 roomIdx = pPT->m_roomIdx;
	Assert(roomIdx > -1);
	Assertf( roomIdx < m_roomData.GetCount(), "interior: %s : cannot add to room %d (only %d rooms exist)",GetModelName(), roomIdx, m_roomData.GetCount());

	if (roomIdx > 0 && (roomIdx < m_roomData.GetCount()) && (!m_roomData[roomIdx].m_activeTrackerList.IsMemberOfList(pPT))) {
		m_roomData[roomIdx].m_activeTrackerList.Add(pPT);
	}
}

// remove from the list of trackers active in this room
void	CInteriorInst::RemoveFromActiveTrackerList(CPortalTracker *pPT){

	s32 roomIdx = pPT->m_roomIdx;

#if __ASSERT
	s32 numRooms = m_roomData.GetCount();
	for(s32 i=0;i<numRooms;i++){
		if (pPT->m_roomIdx != i){
			if ((m_roomData.GetCapacity() != 0) && (m_roomData[i].m_activeTrackerList.IsMemberOfList(pPT))) {
				Assertf(false, "This tracker is referenced in wrong room");
			}
		}
	}

	if ((m_roomData.GetCapacity() > roomIdx)){
		Assert(&m_roomData[roomIdx]);
		Assert(&m_roomData[roomIdx].m_activeTrackerList);
	}
#endif // __ASSERT

	// the portal tracker _should_ only ever be in 1 room - the one it thinks it is in!
	if (m_roomData.GetCapacity() > roomIdx){
		m_roomData[roomIdx].m_activeTrackerList.Remove(pPT ASSERT_ONLY(,false));
	}
}

// Add the active objects (for static collision) in this interior onto the given list
void	CInteriorInst::AppendActiveObjects(atArray<CEntity*>& entityArray){

	// won't be anything to add!
	if (!m_bIsPopulated){
		return;
	}

	Assert( m_bInstanceDataGenerated );

	CEntity* pEntity = NULL;
	RestartEntityInInteriorList();
	while(GetNextEntityInInteriorList(&pEntity))
	{
		if (pEntity && pEntity->GetIsTypeBuilding()){
			if (AssertVerify(entityArray.GetCount() < entityArray.GetCapacity())){
				entityArray.Push(pEntity);
			}
		}
	}

	// render the objects attached to the entry exit portals
	s32 numPortals = GetNumPortalsInRoom(0);
	for(s32 i = 0;i<numPortals;i++)
	{
		u32 globalPortalIdx = GetPortalIdxInRoom(0,i);
		for(u32 j=0; j< m_aPortals[globalPortalIdx]->GetEntityCount(); j++){
			// draw any objects attached to portals we are moving through
			CEntity* pPortalEntity = GetPortalObjInstancePtr(0,i,j);
			if (pPortalEntity){
				if (AssertVerify(entityArray.GetCount() < entityArray.GetCapacity())){
					entityArray.Push(pPortalEntity);
				}
			}
		}
	}
}

// add just the portal active objects to the list
void	CInteriorInst::AppendPortalActiveObjects(atArray<CEntity*>& entityArray){

	Assert( m_bInstanceDataGenerated );

	// append just the objects attached to the entry exit portals
	s32 numPortals = GetNumPortalsInRoom(0);
	for(s32 i = 0;i<numPortals;i++)
	{
		u32 globalPortalIdx = GetPortalIdxInRoom(0,i);
		for(u32 j=0; j< m_aPortals[globalPortalIdx]->GetEntityCount(); j++){
			// draw any objects attached to portals we are moving through
			CEntity* pPortalEntity = GetPortalObjInstancePtr(0,i,j);
			if (pPortalEntity){
				if (AssertVerify(entityArray.GetCount() < entityArray.GetCapacity())){
					entityArray.Push(pPortalEntity);
				}
			}
		}
	}
}

// Add all of the objects in this interior which match the type flags given
void	CInteriorInst::AppendObjectsByType(fwPtrListSingleLink* pMloExtractedObjectList, s32 typeFlags){

	Assert(pMloExtractedObjectList);
	Assert((typeFlags&ENTITY_TYPE_MASK_MLO)==0);		// can't have MLOs inside interiors!
	Assert((typeFlags&ENTITY_TYPE_MASK_PORTAL)==0);	// can't have portals inside interiors!

	// won't be anything to add!
	if (!m_bIsPopulated){
		return;
	}

	Assert( m_bInstanceDataGenerated );

	// - process static objects and dummies in room arrays -

	CEntity* pEntity = NULL;
	RestartEntityInInteriorList();
	while(GetNextEntityInInteriorList(&pEntity))
	{
		if(pEntity)
		{
			if ( ((typeFlags&ENTITY_TYPE_MASK_BUILDING) && (pEntity->GetIsTypeBuilding())) ||
				((typeFlags&ENTITY_TYPE_MASK_DUMMY_OBJECT) && (pEntity->GetIsTypeDummyObject()))		)
			{
				pMloExtractedObjectList->Add(pEntity);
			}
		}
	}

	// - process objects which are attached to portals
	for(s32 i=0; i<m_aPortals.GetCount(); i++){
		for(u32 j=0; j< m_aPortals[i]->GetEntityCount(); j++){
			pEntity = static_cast< CEntity* >( m_aPortals[i]->GetEntity(j) );
			if (pEntity){
				if ( ((typeFlags&ENTITY_TYPE_MASK_BUILDING) && (pEntity->GetIsTypeBuilding())) ||
					((typeFlags&ENTITY_TYPE_MASK_DUMMY_OBJECT) && (pEntity->GetIsTypeDummyObject())) ||
					((typeFlags&ENTITY_TYPE_MASK_OBJECT) && (pEntity->GetIsTypeObject())))
				{
					pMloExtractedObjectList->Add(pEntity);
				}
			}
		}
	}

	// - process actively tracked objects in room lists -
	for(s32 i=0;i<m_roomData.GetCount();i++){
		fwPtrNode* pNode = m_roomData[i].m_activeTrackerList.GetHeadPtr();
		while(pNode != NULL){
			CPortalTracker* pPT =  static_cast<CPortalTracker*>(pNode->GetPtr());
			Assert(pPT);
			pNode = pNode->GetNextPtr();
			CEntity*	pTrackedEntity = pPT->GetOwner();

			if (pTrackedEntity) {

				if (
					((typeFlags&ENTITY_TYPE_MASK_VEHICLE) && pTrackedEntity->GetIsTypeVehicle())	||
					((typeFlags&ENTITY_TYPE_MASK_BUILDING) && pTrackedEntity->GetIsTypeBuilding())	||
					((typeFlags&ENTITY_TYPE_MASK_PED) && pTrackedEntity->GetIsTypePed())			||
					((typeFlags&ENTITY_TYPE_MASK_OBJECT) && pTrackedEntity->GetIsTypeObject())
					) {
						pMloExtractedObjectList->Add(pTrackedEntity);
				} 
			}
		}
	}
}


// move on to the next entity in the 'list' for this interior
bool CInteriorInst::GetNextEntityInInteriorList(CEntity** ppEntity){

	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));		// will fuck with the internal current entity position otherwise

	if (m_currEntListRoom >= m_roomData.GetCount()){
		return false;
	}

	int currEntListRoom = m_currEntListRoom;
	int currEntListIdx = m_currEntListIdx;

	// once we've ran out of entities in this room, go on to the next room
	currEntListIdx++;
	int roomDataCount = m_roomData.GetCount();
	CRoomData *roomDataArray = m_roomData.GetElements();

	while (!roomDataArray[currEntListRoom].GetRoomSceneNode() || currEntListIdx >= (s32)roomDataArray[currEntListRoom].GetRoomSceneNode()->GetEntityCount()){
		currEntListIdx = 0;
		currEntListRoom++;
		if (currEntListRoom >= roomDataCount){
			m_currEntListIdx = currEntListIdx;
			m_currEntListRoom = currEntListRoom;
			return false;
		}
	}

	m_currEntListIdx = currEntListIdx;
	m_currEntListRoom = currEntListRoom;
	*ppEntity = static_cast< CEntity* >( roomDataArray[currEntListRoom].GetRoomSceneNode()->GetEntity(currEntListIdx) );
	return true;
}

void CInteriorInst::AppendRoomContents(u32 roomIdx, atArray<fwEntity*> *pEntityArray){

	Assert(pEntityArray);

	atArray< fwEntity* >		srcEntities;
	m_roomData[roomIdx].GetRoomSceneNode()->GetEntityContainer()->GetEntityArray( srcEntities );

	for(s32 j = 0; j < srcEntities.GetCount(); ++j)
	{
		if (srcEntities[j]){
			pEntityArray->PushAndGrow(srcEntities[j]);
		}
	}
}

// request objects for this interior using the given request flag. 2nd arg determines if all objects
// in interior should be loaded or just the basic ones (minimum set = low LOD + room 0 + exit portal attached)
// use the distance to prevent loading in unnecessary data (too far away for portals)
void	CInteriorInst::RequestObjects(u32 reqFlag, bool bLoadAll)
{
	// can only request objects if they have all been loaded!
	if (GetProxy()->GetCurrentState() < CInteriorProxy::PS_FULL)
	{
		return;
	}

	Assert( m_bInstanceDataGenerated );

	CEntity* pEntity = NULL;

	// exit portal attached objects
	s32	portalObjIdx = GetNumPortalsInRoom(0)-1;
	while(portalObjIdx >= 0){
		u32 globalPortalIdx = GetPortalIdxInRoom(0,portalObjIdx);
		for(u32 i=0; i<m_aPortals[globalPortalIdx]->GetEntityCount(); i++)
		{
			pEntity = 	GetPortalObjInstancePtr(0, portalObjIdx, i);

			if (pEntity){
				pEntity->CreateDrawable();

				//Assert(!pEntity->GetIsTypeLight());		// not expecting to see these attached to portals.
				if (!pEntity->GetIsTypeLight())
				{
					CModelInfo::RequestAssets(pEntity->GetBaseModelInfo(), reqFlag);
				}

			}
		}
		portalObjIdx--;
	}

	// exit portal 
// 	if(pPosn)
// 	{
// 		portalObjIdx = GetNumPortalsInRoom(0)-1;
// 		while(portalObjIdx >= 0){
// 			//Assert(m_aRoomZeroPortalInsts[portalObjIdx]);
// 			if(m_aRoomZeroPortalInsts[portalObjIdx])
// 			{
// 				float portalDistSqrd = (*pPosn - VEC3V_TO_VECTOR3(m_aRoomZeroPortalInsts[portalObjIdx]->GetTransform().GetPosition())).Mag2();
// 				if(portalDistSqrd < pIntInfo->GetInteriorDetailDist()*pIntInfo->GetInteriorDetailDist())
// 					break;
// 			}
// 			portalObjIdx--;
// 		}
// 	}
// 	// If went through all portals and didn't find one near enough then return
// 	if(portalObjIdx < 0)
// 		return;

	// room 0 objects
	s32	roomZeroObjIdx = GetNumObjInRoom(0)-1;
	while(roomZeroObjIdx >= 0){
		pEntity = 	GetObjInstancePtr(0, roomZeroObjIdx);

		if (pEntity){
			pEntity->CreateDrawable();

			if (!pEntity->GetIsTypeLight()) //don't request streaming of light objects in room 0
				CModelInfo::RequestAssets(pEntity->GetBaseModelInfo(), reqFlag);
		}
		roomZeroObjIdx--;
	}

	if (!bLoadAll){
		return;
	}

	// load the rest of the objects in the other rooms
	// ToDo: this bit!

	// load all of the internal portal attached objects
	// ToDo: this bit!
}

//
// name:		CInteriorInst::RequestRoom
// description:	Request all the objects in one room
void CInteriorInst::RequestRoom(u32 roomIdx, u32 streamingFlags)
{
	CEntity* pEntity = NULL;

	if (GetProxy()->GetCurrentState() < CInteriorProxy::PS_FULL)
	{
		return;
	}

	Assert( m_bInstanceDataGenerated );

	Assertf(roomIdx < GetNumRooms(), "CInteriorInst::LoadRoom : Invalid room index %d", roomIdx);		//check room idx is valid

#if GTA_REPLAY
	Vec3V pos;
	GetProxy()->GetPosition(pos);
	CReplayMgr::RecordRoomRequest(GetProxy()->GetNameHash(), CDoorSystem::GenerateHash32(VEC3V_TO_VECTOR3(pos)), roomIdx);
#endif // GTA_REPLAY

	// request static models
	for(u32 i=0; i<GetNumObjInRoom(roomIdx); i++)
	{
		pEntity = GetObjInstancePtr(roomIdx, i);
		if(pEntity && !pEntity->GetIsTypeLight()) //don't request streaming of light objects
		{
			fwModelId entityId = pEntity->GetModelId();
			if ((CModelInfo::GetAssetStreamFlags(entityId) & STRFLAG_CUTSCENE_REQUIRED) == 0)
			{
				pEntity->CreateDrawable();
				pEntity->SetAlpha(255);
				CModelInfo::RequestAssets(entityId, streamingFlags);
			}
		}
	}

	// request tracked models
	// render the tracked objects for the room
	fwPtrList& trackedObjList =  GetTrackedObjects(roomIdx);

	fwPtrNode* pNode = trackedObjList.GetHeadPtr();

	while(pNode != NULL){
		CPortalTracker* pPT = static_cast<CPortalTracker*>(pNode->GetPtr());
		Assert(pPT);
		pNode = pNode->GetNextPtr();

		pEntity = pPT->GetOwner();

		if(!pEntity) continue;

		if(pEntity)
		{
			fwModelId entityId = pEntity->GetModelId();
			if ((CModelInfo::GetAssetStreamFlags(entityId) & STRFLAG_CUTSCENE_REQUIRED) == 0)
			{
				pEntity->CreateDrawable();
				pEntity->SetAlpha(255);
				CModelInfo::RequestAssets(entityId, streamingFlags);
			}
		}
	}

	// request models attached to portals
	s32 numPortals = GetNumPortalsInRoom(roomIdx);

	for(s32 i = 0;i<numPortals;i++)
	{
		u32 globalPortalIdx = GetPortalIdxInRoom(roomIdx,i);
		for(u32 j=0; j< m_aPortals[globalPortalIdx]->GetEntityCount(); j++){
			// draw any objects attached to portals we are moving through
			CEntity* pPortalEntity = GetPortalObjInstancePtr(roomIdx,i,j);
			if(pPortalEntity && !pPortalEntity->GetIsTypeLight()) //don't request streaming of light objects
			{
				fwModelId portalId = pPortalEntity->GetModelId();
				if ((CModelInfo::GetAssetStreamFlags(portalId) & STRFLAG_CUTSCENE_REQUIRED) == 0)
				{
					pPortalEntity->CreateDrawable();
					pPortalEntity->SetAlpha(255);
					CModelInfo::RequestAssets(portalId, streamingFlags);
				}
			}
		}
	}
}

void CInteriorInst::AddRoomZeroModels(strRequestArray<INTLOC_MAX_ENTITIES_IN_ROOM_0>& RoomZeroRequestArray)
{
	CEntity* pEntity = NULL;
	for(u32 i=0; i<GetNumObjInRoom(0); i++)
	{
		pEntity = GetObjInstancePtr(0, i);
		if(pEntity && !pEntity->GetIsTypeLight() && pEntity!=m_pInternalLOD) //don't request streaming of light objects or internal LOD
		{
			RoomZeroRequestArray.PushRequest(pEntity->GetModelIndex(), CModelInfo::GetStreamingModuleId(), STRFLAG_DONTDELETE|STRFLAG_INTERIOR_REQUIRED);
		}
	}
}

void CInteriorInst::HoldRoomZeroModels(CInteriorInst* pIntInst)
{
	if ( (ms_pHoldRoomZeroInterior != pIntInst) )
	{
		if (ms_pHoldRoomZeroInterior != NULL)
		{
			g_MapTypesStore.RemoveRef(strLocalIndex(ms_pHoldRoomZeroInterior->GetMloModelInfo()->GetMapTypeDefIndex()), REF_OTHER);
		}

		ms_roomZeroRequests.ClearRequiredFlags(STRFLAG_INTERIOR_REQUIRED|STRFLAG_DONTDELETE);
		ms_roomZeroRequests.ReleaseAll();

		ms_pHoldRoomZeroInterior = pIntInst;
		if (pIntInst != NULL)
		{
			pIntInst->AddRoomZeroModels(ms_roomZeroRequests);
			ms_roomZeroRequests.SetRequiredFlags(STRFLAG_INTERIOR_REQUIRED|STRFLAG_DONTDELETE);
			g_MapTypesStore.AddRef(strLocalIndex(ms_pHoldRoomZeroInterior->GetMloModelInfo()->GetMapTypeDefIndex()), REF_OTHER);
		}
	}
}


CBaseModelInfo* pModelInfo_DEBUG = NULL;

bool CInteriorInst::SetAlphaForRoomZeroModels(u16 sourceAlpha)
{
	if (GetProxy()->GetCurrentState() < CInteriorProxy::PS_PARTIAL){
		return(false);
	}

	Assert( m_bInstanceDataGenerated );

	u16 lodFadeAlpha = 255;
	u16 detailFadeAlpha = 0;

	if ((GetMLOFlags() & INSTANCE_MLO_SHORT_FADE) == 0)
	{
		detailFadeAlpha = rage::Min(sourceAlpha, u16(128)) * 2;
		detailFadeAlpha     = rage::Min(detailFadeAlpha, u16(255));

		lodFadeAlpha = rage::Min(u16(255 - sourceAlpha), u16(128)) * 2;
		lodFadeAlpha = rage::Min(lodFadeAlpha, u16(255));
	}
	else
	{
		if (sourceAlpha >= 128)
		{
			sourceAlpha = (sourceAlpha - 128) * 2;
			detailFadeAlpha = rage::Min(sourceAlpha, u16(128)) * 2;
			detailFadeAlpha     = rage::Min(detailFadeAlpha, u16(255));

			lodFadeAlpha = rage::Min(u16(255 - sourceAlpha), u16(128)) * 2;
			lodFadeAlpha = rage::Min(lodFadeAlpha, u16(255));
		} else
		{
			detailFadeAlpha = 0;
			lodFadeAlpha = 255;
		}
	}


	u32 numObj = GetNumObjInRoom(0);
	bool bForceLOD = (numObj == 0);				// force LOD, unless we have shell models to draw
	for(u32 i=0; i<numObj; i++)
	{
		CEntity* pEntity = GetObjInstancePtr(0, i);
		pModelInfo_DEBUG = pEntity->GetBaseModelInfo();

		if(pEntity && pEntity!=m_pInternalLOD && !pEntity->GetIsTypeLight()) //don't request streaming of light objects
		{
			if (!pEntity->GetDrawHandlerPtr())
			{
				pEntity->CreateDrawable();

				if (!pEntity->GetArchetype()->GetHasLoaded()){
					bForceLOD = true;			// shell models not ready, reactivate LODs
				}
			}

			// bug 1761516 - the shell of this interior (chicken factory) is being suppressed too early...
			if (pEntity->GetArchetype()->GetModelNameHash() != ATSTRINGHASH("v_34_cb_shell3", 0xf023bce5))
			{
				pEntity->SetAlpha(u8(detailFadeAlpha));
			}
		}
	}


	if (m_pInternalLOD)
	{
		if (bForceLOD)
		{
			//if (m_bAddedToClosedInteriorList)				// may find it necessary to only do this under certain conditions : depends if it causes less or more bugs...
			{
				// request models attached to portals
				s32 numPortals = GetNumPortalsInRoom(0);
				for(s32 i = 0;i<numPortals;i++)
				{
					u32 globalPortalIdx = GetPortalIdxInRoom(0,i);
					for(u32 j=0; j< m_aPortals[globalPortalIdx]->GetEntityCount(); j++){
						// draw any objects attached to portals we are moving through
						CEntity* pPortalEntity = GetPortalObjInstancePtr(0,i,j);
						if(pPortalEntity && !pPortalEntity->GetIsTypeLight()) //don't request streaming of light objects
						{
							pPortalEntity->CreateDrawable();
							pPortalEntity->SetAlpha(LODTYPES_ALPHA_VISIBLE);
						}
					}
				}
			}

			if  (m_pProxy && m_pProxy->GetCurrentState() < CInteriorProxy::PS_FULL)
			{
				return false;
			}
			else
			{
				m_pInternalLOD->SetAlpha(LODTYPES_ALPHA_VISIBLE);
				m_pInternalLOD->GetLodData().SetVisible(true);
			}
		} 
		else 
		{
			m_pInternalLOD->SetAlpha(u8(lodFadeAlpha));
		}
	}

	return(true);
}

void CInteriorInst::CapAlphaForContentsEntities(void)
{
	if ((GetProxy()->GetCurrentState() < CInteriorProxy::PS_PARTIAL) || 
		((GetMLOFlags() & INSTANCE_MLO_CAP_CONTENTS_ALPHA) == 0))
	{
		return;
	}

	Assert( m_bInstanceDataGenerated );

	u16 MLOAlpha = GetAlpha();
	u16 detailFadeAlpha = rage::Min(MLOAlpha, u16(128)) * 2;
	detailFadeAlpha     = rage::Min(detailFadeAlpha, u16(255));

	u32 numRooms = GetNumRooms();
	for(u32 roomIdx = 1; roomIdx < numRooms; roomIdx++)			// everything _except_ room 0 now
	{
		u32 numObj = GetNumObjInRoom(roomIdx);
		for(u32 entityIdx=0; entityIdx<numObj; entityIdx++)
		{
			CEntity* pEntity = GetObjInstancePtr(roomIdx, entityIdx);
			if(pEntity && pEntity!=m_pInternalLOD && !pEntity->GetIsTypeLight()) //don't request streaming of light objects
			{
				pEntity->SetAlpha(MIN(u8(detailFadeAlpha), pEntity->GetAlpha()));
			}
		}
	}
}

void CInteriorInst::SuppressInternalLOD()
{
	if (m_pInternalLOD)
	{
		m_pInternalLOD->SetAlpha(LODTYPES_ALPHA_INVISIBLE);
	}
}

void CInteriorInst::SuppressExternalLOD()
{
	if (GetLod())
	{
		GetLod()->SetAlpha(LODTYPES_ALPHA_INVISIBLE);
	}
}

// return ptr to the portal inst which corresponds to the portal identified by room & portal index for this interior
// return NULL if it doesn't have an associated portal inst
CPortalInst*	CInteriorInst::GetMatchingPortalInst(s32 roomIdx, s32 portalIdx) const {

	if (!m_bIsPopulated){
		return(NULL);
	}

	// this is easy for room 0...
	if (roomIdx == 0){
		Assert(portalIdx < (s32)GetNumPortalsInRoom(0));	// check portal idx is valid for room 0
		return(m_aRoomZeroPortalInsts[portalIdx]);				// portal with idx i in room 0 is the one desired
	}

	// for other rooms, need to search for room 0 portal which matches the portal in the target room

#if __DEV
	u32 destId = GetDestThruPortalInRoom(roomIdx, portalIdx);
	if (0 != GetDestThruPortalInRoom(roomIdx, portalIdx)){
		Warningf("Destination through portal (%d,%d) should have been 0, but was %d. Dump interior:- \n",roomIdx, portalIdx, destId);
		CDebug::DumpEntity(this);
	}
#endif //__DEV

	Assert(0 == GetDestThruPortalInRoom(roomIdx, portalIdx)); // portal must lead outside

	s32 portalArrayIdxCurrRoom = GetPortalIdxInRoom(roomIdx, portalIdx);
	s32 portalArrayIdxRoomZero = -1;

	// it's slightly awkward to find the portalIdx in room 0 which refers to same portal in roomIdx - need to search
	u32 i;
	for(i=0;i<GetNumPortalsInRoom(0);i++){
		portalArrayIdxRoomZero = GetPortalIdxInRoom(0,i);
		if (portalArrayIdxRoomZero == portalArrayIdxCurrRoom){
			return(m_aRoomZeroPortalInsts[i]);				// portal with idx i in room 0 is the one desired
		}
	}

	return(NULL);
}

Vector3 gTestVector;
u32 gTestPosHash = 0;
bool	bUsePosHash =false;

// looking for a MLO at a co-ordinate. If there is one, return its pool index
bool GetInteriorInstancePtrCB(CEntity* pEntity, void* data)
{
	Assert(pEntity);
	CInteriorInst** ppReturnIntInst = static_cast<CInteriorInst**>(data);
	CBaseModelInfo *pModelInfo = pEntity->GetBaseModelInfo();
	Assert(pModelInfo);

	if (pModelInfo->GetModelType() == MI_TYPE_MLO)
	{
		CInteriorInst* pIntInst = (static_cast<CInteriorInst*>(pEntity));

		// if testing using the position hash then don't bother checking for point in room etc.
		if (bUsePosHash)
		{
			if (gTestPosHash == pIntInst->GeneratePositionHash())
			{
				*ppReturnIntInst = pIntInst;
				return(false);
			}
			else
			{
				return(true);
			}
		}

		if ( !pIntInst->m_bIsPopulated )
			return true;

		spdAABB tempBox;
		const spdAABB &bbox = pIntInst->GetBoundBox(tempBox);
		if (bbox.ContainsPoint(RCC_VEC3V(gTestVector))){
			// in bounding box of an interior - but may not be best match...
			*ppReturnIntInst = pIntInst;

			// if we found a good suitable interior then check all rooms - except for room 0
			for (u32 i=1;i<pIntInst->GetNumRooms(); i++){
				spdAABB roomBBox;

				pIntInst->CalcRoomBoundBox(i, roomBBox);

				if (roomBBox.ContainsPoint(RCC_VEC3V(gTestVector))){
					return(false);				// definitely in one of the rooms skip any further searching
				}
			}
		}
	}

	return(true);
}

// get the interior at the given co-ords (if there is one instanced) - matching the position hash
CInteriorInst* CInteriorInst::GetInstancedInteriorAtCoords(const Vector3& testPos, u32 posHash /*=0*/, float fTestRadius /*=0.1f*/)
{
	// test against all MLO objects in the surrounding area...
	CInteriorInst* pIntInst = NULL;

	fwIsSphereIntersecting testSphere(spdSphere(RCC_VEC3V(testPos), ScalarV(fTestRadius)));
	gTestVector = testPos;
	gTestPosHash = posHash;
	bUsePosHash = (posHash != 0);
	CGameWorld::ForAllEntitiesIntersecting(&testSphere, GetInteriorInstancePtrCB, (void*) &pIntInst, 
		ENTITY_TYPE_MASK_MLO, SEARCH_LOCATION_EXTERIORS, SEARCH_LODTYPE_HIGHDETAIL, SEARCH_OPTION_NONE, WORLDREP_SEARCHMODULE_PORTALS);

	gTestPosHash = 0;
	bUsePosHash = false;
	return(pIntInst);
}

extern	CRenderPhaseScanned *g_SceneToGBufferPhaseNew;

// calculate a scaling factor for the daylight fill lights in an interior. Lights scaled up in intensity 
// when entering an interior and are full intensity whilst inside
float	CInteriorInst::GetDaylightFillScale(void) const {
	if (g_SceneToGBufferPhaseNew && g_SceneToGBufferPhaseNew->GetPortalVisTracker()->GetInteriorInst() == this){
		return(1.0f);
	}
	
	if (m_closestExternPortalDist > 4.0f) {
		return(0.0f);
	}

	return(1.0f - (m_closestExternPortalDist / 4.0f));
}

// convert objects attached to portals to real objects
void	CInteriorInst::PortalObjectsConvertToReal(void){

	Assert( m_bInstanceDataGenerated );

	// convert objects attached to this interiors portals
	for(s32 i=0; i<m_aPortals.GetCount();i++){
		for(u32 j=0; j< m_aPortals[i]->GetEntityCount(); j++){
			CEntity* pEntity = static_cast< CEntity* >( m_aPortals[i]->GetEntity(j) );
			if (pEntity){
				if(!CStreaming::IsStreamingDisabled())
				{
					// If model is not loaded
					if(!CModelInfo::HaveAssetsLoaded(pEntity->GetBaseModelInfo()))
					{
						CModelInfo::RequestAssets(pEntity->GetBaseModelInfo(), STRFLAG_FORCE_LOAD);
					}
				}

				if (pEntity->GetIsTypeDummyObject()){
					CDummyObject* pDummyObj = static_cast<CDummyObject*>(pEntity);
					CObjectPopulation::ConvertToRealObject(pDummyObj);
				}
			}
		}
	}

	// also convert objects attached to portals in interiors which are linked to this interior

	// go through all instanced portals
	for(s32 i=0; i<m_aRoomZeroPortalInsts.GetCount();i++){
		// any linked ones then find the destination
		CPortalInst* pPortalInst = m_aRoomZeroPortalInsts[i];
		if (pPortalInst && pPortalInst->HasBeenLinked()){
			// get the portal in the destination which this interior is linking to
			CInteriorInst*	pDest = NULL;
			s32			portalIdx = -1;

			pPortalInst->GetDestinationData(this, pDest, portalIdx);

			if (pDest){
				// get the objects attached to this portal and convert it to real
				for(u32 j=0; j< m_aPortals[i]->GetEntityCount(); j++){

					CEntity* pEntity = pDest->GetPortalObjInstancePtr(0, portalIdx, j);		// portal idx is idx in room 0 (not idx out of portal array)
					if (pEntity && pEntity->GetIsTypeDummyObject()){
						// request the model - these linked ones are likely to be needed
						// If haven't added any objects to priority list this frame
						if(!CStreaming::IsStreamingDisabled())
						{
							// If model is not loaded
							fwModelId entityId = pEntity->GetModelId();
							if(!CModelInfo::HaveAssetsLoaded(entityId))
							{
								CModelInfo::RequestAssets(entityId, STRFLAG_FORCE_LOAD);
								pEntity->SetAlpha(255);
							}
						}

						CDummyObject* pDummyObj = static_cast<CDummyObject*>(pEntity);
						CObjectPopulation::ConvertToRealObject(pDummyObj);
					}
				}
			}
		}
	}

	CPortal::AddToActivePortalObjList(this);
}

// convert objects attached to portals to dummy objects
void	CInteriorInst::PortalObjectsConvertToDummy(void){
	Assert( m_bInstanceDataGenerated );
	Assert(!m_bIsDummied);

	for(s32 i=0; i<m_aPortals.GetCount();i++){
		for(u32 j=0; j< m_aPortals[i]->GetEntityCount(); j++){
			CEntity* pEntity = static_cast< CEntity* >( m_aPortals[i]->GetEntity(j) );
			if (pEntity && pEntity->GetIsTypeObject()){
				CObject* pObject = static_cast<CObject*>(pEntity);
				// just plain don't dummify some objects...
				if (pObject->CanBeDeleted()){
					// convert to dummy object which should automatically get put back in original place
					CObjectPopulation::ConvertToDummyObject(pObject, true);
				}
			}
		}
	}

	CPortal::RemoveFromActivePortalObjList(this);
}

// remove objects attached to portals
void	CInteriorInst::PortalObjectsDestroy(void){
	
	Assert( m_bInstanceDataGenerated );

	//Assert(m_bIsDummied);
	if (!m_bIsDummied){
		Displayf("*** ->Destroying portal objects although interior doesn't appear to be dummied...\n");
	}

	for(s32 i=0; i<m_aPortals.GetCount();i++)
	{
		bool	resetLoop = true;
		ASSERT_ONLY(u32	iterationCount = 0; )

		// Sometimes removing some entities causes other entities to be removed from the same container,
		// so we need to restart the loop
		while (resetLoop)
		{
			resetLoop = false;
			Assertf( ++iterationCount < 10000, "PortalObjectsDestroy iterated more than 10000 times on the entity array. There's probably something wrong with the iteration logic." );

			atArray< fwEntity* >		entities;
			m_aPortals[i]->GetEntityContainer()->GetEntityArray( entities );

			for(s32 j = 0; j < entities.GetCount() && !resetLoop; ++j)
			{
				CEntity* pEntity = static_cast< CEntity* >( entities[j] );
				if (pEntity){

					if ( pEntity->GetIsTypeComposite() ){
						resetLoop = true;
					}

#if GTA_REPLAY
					if(pEntity->GetOwnedBy() == ENTITY_OWNEDBY_REPLAY)
					{
						if(pEntity->GetIsDynamic())
						{
							CDynamicEntity* pDE = static_cast<CDynamicEntity*>(pEntity);
							CGameWorld::Remove(pDE, true);
							pDE->GetPortalTracker()->DontRequestScaneNextUpdate();
							pDE->GetPortalTracker()->StopScanUntilProbeTrue();
							CGameWorld::Add(pDE, CGameWorld::OUTSIDE, true);			// evict into world
							continue;
						}
					}
#endif // GTA_REPLAY

					// it's a problem if I can't clear this up.
#if __ASSERT
					const char *pOwner = "Unknown";
					if (!pEntity->CanBeDeleted())
					{
						switch (pEntity->GetOwnedBy())
						{
						case ENTITY_OWNEDBY_SCRIPT:
							pOwner = "Script";
							break;

						case ENTITY_OWNEDBY_CUTSCENE:
							pOwner = "Cutscene";
							break;

						case ENTITY_OWNEDBY_GAME:
							pOwner = "Game";
							break;

						case ENTITY_OWNEDBY_COMPENTITY:
							pOwner = "CompEntity";
							break;
						default:
							break;
						}
					}
					Assertf(pEntity->CanBeDeleted(), "Failed to clear portal object %s, owned by %s", pEntity->GetModelName(), pOwner);
#endif

					CGameWorld::Remove(pEntity);

					if (pEntity->GetIsTypeObject()){
						// remove dummy from object (allowed to do this, because I created the dummy from interior instancing).
						CObject* pObject = static_cast<CObject*>(pEntity);
						CDummyObject* pDummyObject = pObject->GetRelatedDummy();
						if (pDummyObject){
							CGameWorld::Remove( pDummyObject );
							delete pDummyObject;
							pObject->ClearRelatedDummy();
							resetLoop = true;
						}
						CObjectPopulation::DestroyObject(pObject);
					} else {
						delete pEntity;
					}
				}
			}
		}

		m_aPortals[i]->GetEntityContainer()->RemoveAllEntities();
	}
}

u32 CInteriorInst::GeneratePositionHash(void) const {

	Vector3 pos = VEC3V_TO_VECTOR3(GetTransform().GetPosition());

	pos.y += 4096.0f;
	pos.x += 4096.0f;

	Assert(pos.z >= -99.0f);		// allow z within -100..411m range		(2m accuracy)
	Assert(pos.z <= 411.0f);
	Assert(pos.x >= 0.0f);	// allow x within 0..8192m range	(2m accuracy)
	Assert(pos.x <= 8191.0f);
	Assert(pos.y >= 0.0f);	// allow y within 0..8192m range	(2m accuracy)
	Assert(pos.y <= 8191.0f);

	u32	zPart = (((s32)(rage::Floorf(pos.z + 0.5f))) + 100) / 2;
	u32	yPart = ((s32)(rage::Floorf(pos.y + 0.5f)));
	u32	xPart = ((s32)(rage::Floorf(pos.x + 0.5f)));

	u32 hash = zPart << 24;
	hash = hash | (yPart << 12);
	hash = hash | xPart;

	Assert(hash != 0);

	return(hash);
}

#if __BANK
// get a string which describes the current LOD state of this interior
void	CInteriorInst::GetLODState(char* pString){

	u32	detailDist = GetLodDistance();
	fwEntity* pParent = GetLod();

	char	distString[80];
	char	fadeString[80];

	CEntity* pShellObj = NULL;
	if (m_bIsContentsPopulated)
	{	
		pShellObj = GetObjInstancePtr(0,GetNumObjInRoom(0)-1);		// avoid internal LOD or it's lights...
	}
	u32 shellObjAlpha = ( pShellObj ? pShellObj->GetAlpha() : 0);
	
	if (pParent){
		u32	LOD1Dist = pParent->GetLodDistance();
		sprintf(distString, "[D:%d  -  L1:%d] [Lod:'%s']", detailDist, LOD1Dist, pParent->GetModelName());
		sprintf(fadeString, "MLO:%03d <HD:%03d LI:%03d LE:%03d>", GetAlpha(), shellObjAlpha, (m_pInternalLOD ? m_pInternalLOD->GetAlpha() : 0), pParent->GetAlpha());
	} else {
		sprintf(distString, "[D:%d]", detailDist);
		sprintf(fadeString, "MLO:%03d <HD:%03d>", GetAlpha(), shellObjAlpha);
	}

	sprintf(pString,"<Dist:%.2f Portal:%.2f>  %s  %s",m_MLODist, m_closestExternPortalDist, fadeString, distString);
}
#endif	//	__BANK

#if __SCRIPT_MEM_CALC
void CInteriorInst::AddRoomContents(u32 roomIdx, atArray<u32>& modelIdxArray){

	// add static objects for this room
	for(u32 i=0; i<GetNumObjInRoom(roomIdx); i++)
	{
		CEntity* pEntity = GetObjInstancePtr(roomIdx, i);
		if (pEntity){
			Assert(pEntity->GetModelIndex() != fwArchetypeManager::INVALID_STREAM_SLOT);
			modelIdxArray.PushAndGrow(pEntity->GetModelIndex(), 64);
		}
	}

	// add tracked objects for this room
	fwPtrList& trackedObjList =  GetTrackedObjects(roomIdx);
	fwPtrNode* pNode = trackedObjList.GetHeadPtr();
	while(pNode != NULL){
		CPortalTracker* pPT = static_cast<CPortalTracker*>(pNode->GetPtr());
		Assert(pPT);
		pNode = pNode->GetNextPtr();

		CEntity* pEntity = pPT->GetOwner();

		if(!pEntity ||!pEntity->GetIsTypeObject())
			continue;

		if (pEntity){
			Assert(pEntity->GetModelIndex() != fwArchetypeManager::INVALID_STREAM_SLOT);
			modelIdxArray.PushAndGrow(pEntity->GetModelIndex(), 64);
		}
	}
}

void	CInteriorInst::AddInteriorContents(s32 roomIdx, bool bAllRooms, bool bPortalAttached, atArray<u32>& modelIdxArray){

	Assert( m_bInstanceDataGenerated );

	// portal attached objects only
	if (bPortalAttached){
		// add portal attached objects
		for(s32 i=0; i<m_aPortals.GetCount();i++){
			for(u32 j=0; j< m_aPortals[i]->GetEntityCount(); j++){
				CEntity* pEntity = static_cast< CEntity* >( m_aPortals[i]->GetEntity(j) );
				if (pEntity){
					Assert(pEntity->GetModelIndex() != fwArchetypeManager::INVALID_STREAM_SLOT);
					modelIdxArray.PushAndGrow(pEntity->GetModelIndex());
				}
			}
		}

		return;
	}
	
	// objects in rooms
	if (bAllRooms){
		for(u32 i=0; i< GetNumRooms();i++){
			AddRoomContents(i, modelIdxArray);
		}

		return;
	}

	if (roomIdx >= 0){
		AddRoomContents(roomIdx, modelIdxArray);
	}

}

#endif //__SCRIPT_MEM_CALC



bool
CInteriorInst::GetSmokeLevelGridIds(s32& gridX, s32& gridY, s32 roomIdx, const Vector3& wldPos) const
{
	Vector3 lclPos = VEC3V_TO_VECTOR3(GetTransform().UnTransform(VECTOR3_TO_VEC3V(wldPos)));

	Vector3 bbMin = GetMloModelInfo()->GetBoundingBoxMin();
	Vector3 bbMax = GetMloModelInfo()->GetBoundingBoxMax();

	if (lclPos.x<=bbMin.x ||
		lclPos.x>=bbMax.x ||
		lclPos.y<=bbMin.y ||
		lclPos.y>=bbMax.y ||
		lclPos.z<=bbMin.z ||
		lclPos.z>=bbMax.z)
	{
		return false;
	}

#if __ASSERT
	float width = bbMax.x - bbMin.x;
	float depth = bbMax.y - bbMin.y;

	float gridSizeX = width/MAX_SMOKE_LEVEL_SECTIONS;
	float gridSizeY = depth/MAX_SMOKE_LEVEL_SECTIONS;

	gridSizeX = MAX(gridSizeX, VFXWEAPON_INTSMOKE_MIN_GRID_SIZE);
	gridSizeY = MAX(gridSizeY, VFXWEAPON_INTSMOKE_MIN_GRID_SIZE);

	Assert(m_roomData[roomIdx].m_smokeGridSize == MAX(gridSizeX, gridSizeY));
#endif

	gridX = (s32)((lclPos.x-bbMin.x)/m_roomData[roomIdx].m_smokeGridSize);
	gridY = (s32)((lclPos.y-bbMin.y)/m_roomData[roomIdx].m_smokeGridSize);

	Assertf(gridX>=0, 
		"GridX %d World Pos %.2f %.2f %.2f Local Pos %.2f %.2f %.2f BBMin %.2f %.2f %.2f BBMax %.2f %.2f %.2f RoomIndex %d GridSize %.2f", gridX, wldPos.x, wldPos.y, wldPos.z, lclPos.x, lclPos.y, lclPos.z, bbMin.x, bbMin.y, bbMin.z, bbMax.x, bbMax.y, bbMax.z, roomIdx, m_roomData[roomIdx].m_smokeGridSize);
	Assertf(gridX<=MAX_SMOKE_LEVEL_SECTIONS, 
		"GridX %d World Pos %.2f %.2f %.2f Local Pos %.2f %.2f %.2f BBMin %.2f %.2f %.2f BBMax %.2f %.2f %.2f RoomIndex %d GridSize %.2f", gridX, wldPos.x, wldPos.y, wldPos.z, lclPos.x, lclPos.y, lclPos.z, bbMin.x, bbMin.y, bbMin.z, bbMax.x, bbMax.y, bbMax.z, roomIdx, m_roomData[roomIdx].m_smokeGridSize);
	Assertf(gridY>=0, 
		"GridY %d World Pos %.2f %.2f %.2f Local Pos %.2f %.2f %.2f BBMin %.2f %.2f %.2f BBMax %.2f %.2f %.2f RoomIndex %d GridSize %.2f", gridY, wldPos.x, wldPos.y, wldPos.z, lclPos.x, lclPos.y, lclPos.z, bbMin.x, bbMin.y, bbMin.z, bbMax.x, bbMax.y, bbMax.z, roomIdx, m_roomData[roomIdx].m_smokeGridSize);
	Assertf(gridY<=MAX_SMOKE_LEVEL_SECTIONS, 
		"GridY %d World Pos %.2f %.2f %.2f Local Pos %.2f %.2f %.2f BBMin %.2f %.2f %.2f BBMax %.2f %.2f %.2f RoomIndex %d GridSize %.2f", gridY, wldPos.x, wldPos.y, wldPos.z, lclPos.x, lclPos.y, lclPos.z, bbMin.x, bbMin.y, bbMin.z, bbMax.x, bbMax.y, bbMax.z, roomIdx, m_roomData[roomIdx].m_smokeGridSize);

	// make sure the indices are clamped to avoid writing to invalid elements later
	// note that the above asserts check for <=MAX_SMOKE_LEVEL_SECTIONS instead of <MAX_SMOKE_LEVEL_SECTIONS
	// to avoid asserts when there are slight inaccuracies
	gridX = Clamp(gridX, 0, MAX_SMOKE_LEVEL_SECTIONS-1);
	gridY = Clamp(gridY, 0, MAX_SMOKE_LEVEL_SECTIONS-1);

	return true;
}

void	
CInteriorInst::AddSmokeToRoom(s32 roomIdx, const Vector3& wldPos, bool isExplosion) 
{
	if (isExplosion)
	{
		for (s32 j=0; j<MAX_SMOKE_LEVEL_SECTIONS; j++)
		{
			for (s32 k=0; k<MAX_SMOKE_LEVEL_SECTIONS; k++)
			{
				m_roomData[roomIdx].m_smokeLevel[j][k] = MIN(VFXWEAPON_INTSMOKE_MAX_LEVEL, m_roomData[roomIdx].m_smokeLevel[j][k]+VFXWEAPON_INTSMOKE_INC_PER_EXP);
			}
		}
	}
	else
	{
		s32 gridX;
		s32 gridY;
		if (GetSmokeLevelGridIds(gridX, gridY, roomIdx, wldPos))
		{
			m_roomData[roomIdx].m_smokeLevel[gridX][gridY] = MIN(VFXWEAPON_INTSMOKE_MAX_LEVEL, m_roomData[roomIdx].m_smokeLevel[gridX][gridY]+VFXWEAPON_INTSMOKE_INC_PER_SHOT);
		}
	}
}


float	
CInteriorInst::GetRoomSmokeLevel(s32 roomIdx, const Vector3& wldPos, const Vector3& wldFwd) const
{
	if (!m_bIsPopulated){
		return(0.0f);
	}

	Vector3 wldFwdTemp = wldFwd;
	wldFwdTemp.z = 0.0f;
	wldFwdTemp.Normalize();
	wldFwdTemp *= m_roomData[roomIdx].m_smokeGridSize;

	Vector3 wldRight(wldFwdTemp.y, -wldFwdTemp.x, 0.0f);

	Vector3 offsets[5];
	offsets[0] = Vector3(0.0f, 0.0f, 0.0f);
	offsets[1] = wldFwdTemp;
	offsets[2] = wldFwdTemp + wldFwdTemp;
	offsets[3] = wldFwdTemp + wldRight;
	offsets[4] = wldFwdTemp - wldRight;

	float smokeLevel = 0.0f;
	for (s32 i=0; i<5; i++)
	{
		s32 gridX;
		s32 gridY;

		Vector3 testPos = wldPos+offsets[i];
		if (GetSmokeLevelGridIds(gridX, gridY, roomIdx, testPos))
		{
			smokeLevel = MAX(smokeLevel, m_roomData[roomIdx].m_smokeLevel[gridX][gridY]);
		}
	}

	return smokeLevel;
}

// if the water level is set for this room then return true & updated param to reflect the local water level
bool CInteriorInst::GetLocalWaterLevel(s32 roomIdx, float& localWaterLevel) const
{
	// check if the current room has a water level set
	if (!m_bContainsWater){
		return(false);
	}

	if ((roomIdx < m_roomData.GetCount()) && (m_roomData[roomIdx].m_bHasLocalWaterLevel)){
		localWaterLevel = m_roomData[roomIdx].m_localWaterLevel;
		return(true);
	} else {
		// if not, then check if there is a water level set for the entire interiors (ie. for room 0)
		if (m_roomData[0].m_bHasLocalWaterLevel){
			localWaterLevel = m_roomData[0].m_localWaterLevel;
			return(true);
		} else {
			// else make sure we appear 'dry' (there is water somewhere in this interior, but not for this room!)
			localWaterLevel = -2000.0f;
			return(true);
		}
	}
}


// test if the external doors attached to an interior are locked or closed
//bool CInteriorInst::AreDoorsLocked(){
bool CInteriorInst::TestPortalState(bool bIsClosed, bool bIsLocked, s32 globalPortalIdx) const {

	if ( GetProxy()->GetCurrentState() < CInteriorProxy::PS_PARTIAL )
		return(false);

	CMloModelInfo* pMloModelInfo = GetMloModelInfo();
	sceneAssert(pMloModelInfo);

	CPortalFlags portalFlags;

	sceneAssert(pMloModelInfo->GetIsStreamType());
	portalFlags = pMloModelInfo->GetPortalFlags(globalPortalIdx);

	if ( portalFlags.GetIsLinkPortal() )
		return(false);

	if ( !portalFlags.GetAllowClosing() )
		return false;

	Assert( m_bInstanceDataGenerated );

	s32 numDoors = 0;

	// test each entity in this portal for being locked or closed
	for(u32 j=0; j<m_aPortals[globalPortalIdx]->GetEntityCount(); j++)
	{
		CEntity* pEntity = reinterpret_cast< CEntity* >( m_aPortals[globalPortalIdx]->GetEntity(j) );
		if(pEntity && pEntity->GetIsTypeObject() && static_cast<CObject*>(pEntity)->IsADoor())
		{
			CDoor* pDoor = static_cast<CDoor*>(pEntity);

			numDoors++;
			// found a door which doesn't have a locked status, or it's status is unlocked
			if (bIsLocked){

				CDoorSystemData* pDoorData = CDoorSystem::GetDoorData(pDoor);
				if (!pDoorData || pDoorData->GetState() == DOORSTATE_INVALID || !pDoorData->GetLocked())
				{
					return(false);
				}
			}

			// found a door which isn't shut
			if (bIsClosed){
				// Note: it's important that we use the same function here as what
				// CDoor uses internally, when deciding if it should call TestPortalState().
				if(!pDoor->IsShutForPortalPurposes()){
					return(false);
				}
			}
		}
	}

	if (numDoors == 0){
		return(false);
	} else { 
		return(true);		// all the doors which were found satisfied the closed or locked criteria
	}
}

bool CInteriorInst::TestPortalState(bool bIsClosed, bool bIsLocked, s32 roomIdx, s32 portalIdx) const {
	return TestPortalState( bIsClosed, bIsLocked, GetPortalIdxInRoom(roomIdx, portalIdx) );
}

CEntity* CInteriorInst::GetCurrEntityFromPTNode(fwPtrNode* pNode){
	if (pNode){
		CPortalTracker* pTracker = static_cast<CPortalTracker*>(pNode->GetPtr());
		if (pTracker){
			return(pTracker->GetOwner());
		}
	} 

	return(NULL);
}

// go through the pool and return the list of intersections
void CInteriorInst::FindIntersectingMLOs(const fwIsSphereIntersecting &testSphere, fwPtrListSingleLink&	scanList){

	CInteriorInst* pIntInst = NULL;
	CInteriorInst* pIntInstNext = NULL;
	s32 poolSize=CInteriorInst::GetPool()->GetSize();

	pIntInstNext = CInteriorInst::GetPool()->GetSlot(0);

	s32 i = 0;
	while(i<poolSize)
	{
		pIntInst = pIntInstNext;

		// spin to next valid entry
		while((++i < poolSize) && (CInteriorInst::GetPool()->GetSlot(i) == NULL));

		if (i != poolSize)
			pIntInstNext = CInteriorInst::GetPool()->GetSlot(i);

		// do any required processing on each interior
		if (pIntInst)
		{
			spdAABB tempBox;
			spdSphere sphere;
			const spdAABB &box = pIntInst->GetBoundBox( tempBox );
			pIntInst->GetBoundSphere( sphere );

			if (testSphere( sphere, box )){
				scanList.Add(pIntInst);
			}
		}
	}
}

void CInteriorInst::InitializeSubSceneGraph()
{
	Assert( m_bInstanceDataGenerated );

	CMloModelInfo*	pModelInfo = GetMloModelInfo();
	Assert( pModelInfo );

	// handle old format correctly
	sceneAssert(pModelInfo->GetIsStreamType());

	m_interiorSceneNode = rage_new fwInteriorSceneGraphNode;
	m_interiorSceneNode->SetInteriorIndex( static_cast<s16>( CInteriorInst::GetPool()->GetJustIndex( this ) ) );
	fwWorldRepMulti::GetSceneGraphRoot()->AddChild( m_interiorSceneNode );

	for (s32 i = 0; i < m_roomData.GetCount(); i++)
	{
		fwInteriorLocation	interiorLocation;
		interiorLocation = CInteriorProxy::CreateLocation( GetProxy(), i);

		fwRoomSceneGraphNode*		roomSceneNode = rage_new fwRoomSceneGraphNode( m_interiorSceneNode );
		roomSceneNode->SetInteriorLocation( interiorLocation );
		Assertf(pModelInfo->GetRooms()[i].m_exteriorVisibiltyDepth >= -1 && pModelInfo->GetRooms()[i].m_exteriorVisibiltyDepth < 1000, "Error invalid interior visibility depth %d", pModelInfo->GetRooms()[i].m_exteriorVisibiltyDepth);
		roomSceneNode->SetExteriorVisibilityDepth( pModelInfo->GetRooms()[i].m_exteriorVisibiltyDepth );

		roomSceneNode->SetDirectionalLightEnabled( (pModelInfo->GetRooms()[i].m_flags & ROOM_FORCE_DIR_LIGHT_ON) != 0);
		roomSceneNode->SetExteriorDisabled((pModelInfo->GetRooms()[i].m_flags & ROOM_DONT_RENDER_EXTERIOR) != 0);

		m_roomData[i].SetRoomSceneNode( roomSceneNode );
		m_interiorSceneNode->AddChild( roomSceneNode );

		// hack to make all rooms in v_trailer potentially visible to mirror (this should be fixed on the art side)
		if (pModelInfo->GetModelNameHash() == ATSTRINGHASH("v_trailer", 0x6438554B) ||
			pModelInfo->GetModelNameHash() == ATSTRINGHASH("v_trailertrash", 0x93531BDF) ||
			pModelInfo->GetModelNameHash() == ATSTRINGHASH("v_trailertidy", 0x2BB85EA))
		{
			const_cast<atArray<CMloRoomDef>&>(pModelInfo->GetRooms())[i].m_flags |= ROOM_MIRROR_POTENTIALLY_VISIBLE;
		}
	}

	u32 numPortals = pModelInfo->GetPortals().GetCount();
	m_aPortals.Reset();
	m_aPortals.Resize(numPortals);
	const float inv100 = 1.0f / 100.0f;
	for(u32 i=0;i<numPortals;i++)
	{
		const CMloPortalDef& portalData = pModelInfo->GetPortals()[i];

		fwPortalCorners		corners;
		u32					portalRoom1, portalRoom2;
		fwInteriorLocation	interiorLocation;

		for(u32 j=0; j<4; j++)
		{
			Vec3V corner = VECTOR3_TO_VEC3V(portalData.m_corners[j]);
			corner = Transform(GetMatrix(), corner);
			corners.SetCorner(j, VEC3V_TO_VECTOR3(corner));
			m_boundBox.GrowPoint( corner );
		}

		portalRoom1 = portalData.m_roomFrom;
		portalRoom2 = portalData.m_roomTo;

		interiorLocation = CInteriorProxy::CreateLocation(GetProxy(), -1);
		interiorLocation.SetPortalIndex( i );

		m_aPortals[i] = rage_new fwPortalSceneGraphNode( m_interiorSceneNode );
		m_aPortals[i]->SetInteriorLocation( interiorLocation );
		m_aPortals[i]->SetCorners( corners ); // NOTE -- this must happen _before_ SetOpacity and other flag setting because we store stuff in corners w-components
		m_aPortals[i]->SetOpacity( portalData.m_opacity >= 100 ? 1.0f : float(portalData.m_opacity) * inv100 );
		m_aPortals[i]->SetIsOneWay( (portalData.m_flags & INTINFO_PORTAL_ONE_WAY) != 0 );
		m_aPortals[i]->SetIsLinkPortal( (portalData.m_flags & INTINFO_PORTAL_LINK) != 0 );
		m_aPortals[i]->SetRenderLodsOnly( (portalData.m_flags & INTINFO_PORTAL_LOWLODONLY) != 0 );
		m_aPortals[i]->SetAllowClosing( (portalData.m_flags & INTINFO_PORTAL_ALLOWCLOSING) != 0 );

		if (portalData.m_flags & INTINFO_PORTAL_MIRROR)
		{
			if (AssertVerify(portalData.m_roomFrom < pModelInfo->GetRooms().GetCount()))
			{
				const_cast<atArray<CMloRoomDef>&>(pModelInfo->GetRooms())[portalData.m_roomFrom].m_flags |= ROOM_MIRROR_POTENTIALLY_VISIBLE;
			}

			if (portalData.m_flags & INTINFO_PORTAL_MIRROR_FLOOR)
			{
				if (portalData.m_flags & INTINFO_PORTAL_WATER_SURFACE)
				{
#if __ASSERT
					const Vec3V p0 = VECTOR3_TO_VEC3V(corners.GetCorner(0));
					const Vec3V p1 = VECTOR3_TO_VEC3V(corners.GetCorner(1));
					const Vec3V p2 = VECTOR3_TO_VEC3V(corners.GetCorner(2));

					const Vec3V normal = Normalize(Cross(p1 - p0, p2 - p0));

					if (AssertVerify(portalRoom1 < pModelInfo->GetRooms().GetCount()))
					{
						Assertf(Abs<float>(Abs<float>(normal.GetZf()) - 1.0f) < 0.01f, "water surface portal in %s is not horizontal (normal is %f,%f,%f)", pModelInfo->GetRooms()[portalRoom1].m_name.c_str(), VEC3V_ARGS(normal));
					}
#endif // __ASSERT
					m_aPortals[i]->SetMirrorType(fwPortalSceneGraphNode::SCENE_NODE_MIRROR_TYPE_WATER_SURFACE);
				}
				else
				{
					m_aPortals[i]->SetMirrorType(fwPortalSceneGraphNode::SCENE_NODE_MIRROR_TYPE_MIRROR_FLOOR);
				}
			}
			else
			{
				m_aPortals[i]->SetMirrorType(fwPortalSceneGraphNode::SCENE_NODE_MIRROR_TYPE_MIRROR);
				m_aPortals[i]->SetMirrorPriority((u8)portalData.m_mirrorPriority);
			}

			m_aPortals[i]->SetMirrorCanSeeDirectionalLight( (portalData.m_flags & INTINFO_PORTAL_MIRROR_CAN_SEE_DIRECTIONAL_LIGHT) != 0 );
			m_aPortals[i]->SetMirrorCanSeeExteriorView( (portalData.m_flags & INTINFO_PORTAL_MIRROR_CAN_SEE_EXTERIOR_VIEW) != 0 );
		}
		else
		{
			m_aPortals[i]->SetMirrorType(fwPortalSceneGraphNode::SCENE_NODE_MIRROR_TYPE_NONE);
		}

		Assertf( portalRoom1 != 0 || portalRoom2 != 0, "Both ends of this portal are marked as room 0, this is meaningless" );

		if ( portalRoom1 == 0 )
			std::swap( portalRoom1, portalRoom2 );

		m_aPortals[i]->SetNegativePortalEnd( m_roomData[ portalRoom1 ].GetRoomSceneNode() );

		bool portalToExterior = false;
		if ( portalRoom2 != 0 )
			m_aPortals[i]->SetPositivePortalEnd( m_roomData[ portalRoom2 ].GetRoomSceneNode() );
		else
		{
			//if ( portalInfo.GetIsLinkPortal() )
			if ((portalData.m_flags & INTINFO_PORTAL_LINK) != 0)
				m_aPortals[i]->SetPositivePortalEnd( NULL );	// to be filled when the linked interior becomes alive
			else
			{
				m_aPortals[i]->SetPositivePortalEnd( fwWorldRepMulti::GetSceneGraphRoot() );
				portalToExterior = true;
			}
		}



		m_interiorSceneNode->AddChild( m_aPortals[i] );
	}

	m_bArePortalsCreated = true;
}

void CInteriorInst::ProcessCloseablePortals()
{
	if (GetProxy()->GetGroupId() != 0 )
	{
		return;
	}

	sceneAssert( m_bInstanceDataGenerated );

	CMloModelInfo*	pModelInfo = GetMloModelInfo();
	Assert( pModelInfo );

	s32 interiorProxyIndex = CInteriorProxy::GetPool()->GetJustIndex(GetProxy());
	Assertf(interiorProxyIndex >= 0, "Interior index was negative");

	for(u32 portalIdx=0; portalIdx < m_aPortals.GetCount(); portalIdx++)
	{
		if (gForceClosablePortalsShutOnInit)
		{
			const CMloPortalDef& portalData = pModelInfo->GetPortals()[portalIdx];
			bool	bPortalToExterior = ((portalData.m_roomFrom == 0) || (portalData.m_roomTo == 0));

			// if its portal to the exterior and its not one way 
			if (bPortalToExterior && !(portalData.m_flags & INTINFO_PORTAL_ONE_WAY)) 
			{
				const fwPortalSceneGraphNode *pPortalNode = m_aPortals[portalIdx];
				const int entityCount = pPortalNode->GetEntityCount();
				// If allow closing and its portal container contains no entities
				if ((portalData.m_flags & INTINFO_PORTAL_ALLOWCLOSING) && 
					(entityCount != 0))
				{
					fwEntityContainer *pEntityContainer = pPortalNode->GetEntityContainer();
					bool anyHidden = false;

					for (int i = 0; i < entityCount; ++i)
					{
						CEntity *pEntity = static_cast<CEntity*>(pEntityContainer->GetEntity(i));
						if (!pEntity->GetIsVisible())
						{
							anyHidden = true;
							break;
						}
					}

					if (!anyHidden)
					{
						m_aPortals[portalIdx]->Enable(false);
					}
				}
				else
				{
					IncrementmOpenPortalsCount(interiorProxyIndex);
				}
			}
		}
	}

	if (interiorProxyIndex != -1 && m_openPortalsCount == 0)
	{
#if __ASSERT
		int index = fwScan::GetScanBaseInfo().m_closedInteriors.Find((u16)interiorProxyIndex);
		Assertf(index == -1, "Error interior index is already in closed interiors array");
#endif
		if (fwScan::GetScanBaseInfo().m_closedInteriors.GetCount() < fwScanBaseInfo::MAX_CLOSED_INTERIORS)
		{
			fwScan::GetScanBaseInfo().m_closedInteriors.Append() = (u16)interiorProxyIndex;
			m_bAddedToClosedInteriorList = true;
		}
#if __ASSERT
		else
		{
			Assertf(0, "Closed interior list exhausted");
		}
#endif
	}
}

void CInteriorInst::ShutdownSubSceneGraph()
{
	Assert( m_bInstanceDataGenerated );

	for (s32 i = 0; i < m_aPortals.GetCount(); i++)
	{
		fwPortalSceneGraphNode*		portalSceneNode = m_aPortals[i];

		portalSceneNode->GetEntityContainer()->RemoveAllEntities();
		m_interiorSceneNode->RemoveChild( portalSceneNode );

		delete portalSceneNode;
	}

	m_aPortals.Reset();

	for (s32 i = 0; i < m_roomData.GetCount(); i++)
	{
		fwRoomSceneGraphNode*		roomSceneNode = m_roomData[i].GetRoomSceneNode();

		roomSceneNode->GetEntityContainer()->RemoveAllEntities();
		m_interiorSceneNode->RemoveChild( roomSceneNode );

		delete roomSceneNode;
		m_roomData[i].SetRoomSceneNode( NULL );
	}

	fwWorldRepMulti::GetSceneGraphRoot()->RemoveChild( m_interiorSceneNode );
	delete m_interiorSceneNode;
	m_interiorSceneNode = NULL;

	if (GetProxy()->GetGroupId() == 0 && m_openPortalsCount == 0 && m_bAddedToClosedInteriorList)
	{
		s32 interiorProxyIndex = CInteriorProxy::GetPool()->GetJustIndex(GetProxy());
		Assertf(interiorProxyIndex >= 0, "Interior index was negative");

		int index = fwScan::GetScanBaseInfo().m_closedInteriors.Find((u16)interiorProxyIndex); 		
		Assertf(index >= 0, "Negative index for closed interior array"); 

		fwScan::GetScanBaseInfo().m_closedInteriors.Delete(index);
		m_bAddedToClosedInteriorList = false;
	}
}

fwInteriorLocation CInteriorInst::CreateLocation(const CInteriorInst* interiorInst, const s32 roomIndex)
{
	fwInteriorLocation result;
	CInteriorProxy* pInteriorProxy = interiorInst ? interiorInst->GetProxy() : NULL;
	result = CInteriorProxy::CreateLocation(pInteriorProxy, roomIndex);
	return result;
}

CInteriorInst* CInteriorInst::GetInteriorForLocation(const fwInteriorLocation& intLoc)
{ 
	CInteriorProxy* pInteriorProxy = CInteriorProxy::GetFromLocation(intLoc);
	if ( pInteriorProxy )
		return pInteriorProxy->GetInteriorInst();
	else
		return NULL;
}

// for a given position find the interior 
bool CInteriorInst::RequestInteriorAtLocation(Vector3& pos, CInteriorProxy* &foundInteriorProxy, s32& probedRoomIdx){

	// build list of interior proxies intersecting target position
	fwIsSphereIntersecting intersection(spdSphere(RCC_VEC3V(pos), ScalarV(V_FLT_SMALL_1)));
	fwPtrListSingleLink	scanList;
	CInteriorProxy::FindActiveIntersecting(intersection, scanList);

	fwPtrNode* pNode = scanList.GetHeadPtr();

	// no interior intersects this position
	if (pNode == NULL){
		foundInteriorProxy = NULL;
		return(true);
	}

	bool	bAreStaticCollisionsLoaded = true;
	// try to activate all the collisions for these interiors
	while(pNode != NULL)
	{
		// for each found proxy, pull in the .imap file and collisions
		CInteriorProxy* pIntProxy = reinterpret_cast<CInteriorProxy*>(pNode->GetPtr());
		if (pIntProxy && pIntProxy->GetStaticBoundsSlotIndex()!=-1 && !pIntProxy->GetIsCappedAtPartial() && !pIntProxy->GetIsDisabled())
		{
			pIntProxy->SetRequiredByLoadScene(true);
			pIntProxy->RequestContainingImapFile();
			pIntProxy->RequestStaticBoundsFile();

			if (!pIntProxy->AreStaticCollisionsLoaded())
			{
				bAreStaticCollisionsLoaded = false;
			}
		}

		pNode = pNode->GetNextPtr();
	}

	if (!bAreStaticCollisionsLoaded)
	{
		return(false);
	}

	// all intersecting interior proxies now have active collisions, so probe for data
	CInteriorProxy* targetInteriorProxy = NULL;
	probedRoomIdx =	-1;

	bool bProbeRet = CPortalTracker::ProbeForInteriorProxy(pos, targetInteriorProxy, probedRoomIdx, NULL, CPortalTracker::FAR_PROBE_DIST);

	if (bProbeRet && (targetInteriorProxy!=NULL)){
		foundInteriorProxy = targetInteriorProxy;
	}

	//////////////////////////////////////////////////////////////////////////
	// restore imap pin state
	pNode = scanList.GetHeadPtr();
	while(pNode != NULL)
	{
		CInteriorProxy* pIntProxy = reinterpret_cast<CInteriorProxy*>(pNode->GetPtr());
		if (pIntProxy && pIntProxy!=foundInteriorProxy)
		{
			pIntProxy->SetRequiredByLoadScene(false);
		}
		pNode = pNode->GetNextPtr();
	}
	//////////////////////////////////////////////////////////////////////////

	return(true);
}

void CInteriorInst::PatchUpLODDistances(fwEntity* pLOD)
{
	if (pLOD)
	{
		// cap interior fading to population distance to avoid pops
		u32 cappedFadeDist = MIN((u32)(GetProxy()->GetPopulationDistance(false) - 20.0f), GetLodDistance());

		SetLodDistance(cappedFadeDist);		
		pLOD->GetLodData().SetChildLodDistance(cappedFadeDist);		
	}
}

// --fwMapData additions

void CInteriorInst::InitEntityFromDefinition(fwEntityDef* definition, fwArchetype* archetype, s32 mapDataDefIndex)
{
	// quaternion is getting mangled in CEntity::InitEntityFromDefinition - have to restore it
	CBuilding::InitEntityFromDefinition(definition, archetype, mapDataDefIndex);

	// --------------------------------------------------------------------------------
	CMloInstanceDef& def = *smart_cast<CMloInstanceDef*>(definition);
	//CMloModelInfo& mloModel =  *smart_cast<CMloModelInfo*>(archetype);

	// attach to interior proxy with this mapDataDefIndex
	CInteriorProxy* pIntProxy = CInteriorProxy::FindProxy(mapDataDefIndex);
	sceneAssertf(pIntProxy, "Interior doesn't have an associated proxy : %s (%d)",GetModelName(), mapDataDefIndex);
	pIntProxy->SetInteriorInst(this);
	SetProxy(pIntProxy);

	Quaternion q(definition->m_rotation);
	Vector3 p = definition->m_position;

	// use this as the interiorInst position
	SetInstanceTransform(p, q);
	CalcTransformedBounds();

	SetGroupId( def.m_groupId );
	SetFloorId( (s16)def.m_floorId );

	SetMLOFlags( def.m_MLOInstflags); 

	for(u32 i=0; i<def.m_defaultEntitySets.GetCount(); i++)
	{
		pIntProxy->ActivateEntitySet(def.m_defaultEntitySets[i]);
	}
	pIntProxy->ValidateActiveSets();

	//HACK_GTAV
	m_bIgnoreInstancePriority = (pIntProxy && pIntProxy->GetNameHash()==ATSTRINGHASH("milo_replay", 0x3e9967cc));
}

void CInteriorInst::IncrementmOpenPortalsCount(s32 interiorProxyIndex) 
{ 
	if (GetProxy()->GetGroupId() == 0)
	{
		Assertf(interiorProxyIndex >= 0, "InteriorIndex is negative");
		if (m_openPortalsCount == 0 && m_bAddedToClosedInteriorList)
		{
			int index = fwScan::GetScanBaseInfo().m_closedInteriors.Find((u16)interiorProxyIndex);
			fwScan::GetScanBaseInfo().m_closedInteriors.DeleteFast(index);
			m_bAddedToClosedInteriorList = false;
		}

		++m_openPortalsCount; 
	}
}

void CInteriorInst::DecrementmOpenPortalsCount(s32 interiorProxyIndex) 
{ 
	if (GetProxy()->GetGroupId() == 0)
	{
		Assertf(interiorProxyIndex >= 0, "InteriorIndex is negative");

		--m_openPortalsCount;
		Assertf(m_openPortalsCount >= 0, "Negative portal open count"); 

		if (m_openPortalsCount == 0 && !m_bAddedToClosedInteriorList)
		{
			if (fwScan::GetScanBaseInfo().m_closedInteriors.GetCount() < fwScanBaseInfo::MAX_CLOSED_INTERIORS)
			{
				fwScan::GetScanBaseInfo().m_closedInteriors.Append() = (u16)interiorProxyIndex;
				m_bAddedToClosedInteriorList = true;
			}
#if __ASSERT
			else
			{
				Assertf(0, "Closed interior list exhausted");
			}
#endif

		}
	}
}
