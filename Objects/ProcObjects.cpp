///////////////////////////////////////////////////////////////////////////////
//
//	FILE: 	"ProcObjects.cpp"
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	10 March 2004
//	WHAT:	Routines to handle procedurally created objects
//
///////////////////////////////////////////////////////////////////////////////
//
//	19.07.05 - MN - updated to work with rage engine
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "ProcObjects.h"

// rage
#include "bank/bkmgr.h"
#include "grcore/debugdraw.h"
#include "math/random.h"
#include "physics/iterator.h"
#include "system/criticalsection.h"

// game
#include "Camera/CamInterface.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "Network/NetworkInterface.h"
#include "Objects/Object.h"
#include "Physics/GtaInst.h"
#include "Physics/GtaMaterialManager.h"
#include "Physics/Physics.h"
#include "Renderer/PlantsMgr.h"
#include "Scene/Building.h"
#include "Scene/FileLoader.h"
#include "Scene/InstancePriority.h"
#include "Scene/World/GameWorld.h"
#include "Streaming/Streaming.h"
#include "Shaders/CustomShaderEffectBase.h"
#include "Shaders/CustomShaderEffectTint.h"
#include "TimeCycle/TimeCycleConfig.h"


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

#define DEFAULT_MIN_RADIUS_FOR_INCLUSION_IN_HEIGHT_MAP		(0.4f)

///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

CProcObjectMan					g_procObjMan;
static sysCriticalSectionToken	listAccessToken;

///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CProcObjectMan
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Constructor
///////////////////////////////////////////////////////////////////////////////

					CProcObjectMan::CProcObjectMan			()
{
#if __BANK
	m_printNumObjsSkipped = false;
#endif
}


///////////////////////////////////////////////////////////////////////////////
//  Destructor
///////////////////////////////////////////////////////////////////////////////

					CProcObjectMan::~CProcObjectMan			()
{
}


///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

bool				CProcObjectMan::Init					()
{
	// fill up the object pool
	for (s32 i=0; i<MAX_ENTITY_ITEMS; i++)
	{
		m_entityItems[i].m_pData = NULL;
		m_entityItems[i].m_pParentTri = NULL;
		m_entityItems[i].m_pParentEntity = NULL;
		m_entityItems[i].m_allocatedMatrix = false;

		m_entityPool.AddItem(&m_entityItems[i]);
	}

	m_numAllocatedMatrices = 0;
	m_numProcObjCObjects = 0;
	m_numProcObjCBuildings = 0;

	// initialise the segments and intersections
	for (s32 i=0; i<MAX_SEGMENTS; i++)
	{
		m_pSegments[i] = &m_segments[i];
		m_pIntersections[i] = &m_intersections[i];
	}

#if !PSN_PLANTSMGR_SPU_UPDATE
	// set the size of the add and remove arrays
	m_objectsToCreate.Reserve(MAX_ENTITY_ITEMS);	// necessary only for non-SPU update
#endif

#if PLANTSMGR_MULTI_UPDATE
	m_trisToRemove.Reserve(MAX_REMOVED_TRIS);
#endif

#if __DEV
	m_removingProcObject = false;
#endif

#if __BANK
	m_disableEntityObjects = false;
	m_disableCollisionObjects = false;
	m_renderDebugPolys = false;
	m_renderDebugZones = false;
	m_ignoreMinDist = false;
	m_ignoreSeeding = false;
	m_enableSeeding = false;
	m_forceOneObjPerTri = false;
	m_bkMaxProcObjCObjects = 0;
	m_bkNumProcObjsSkipped = 0;
#endif // __BANK

	return true;
}


///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void				CProcObjectMan::Shutdown				()
{
#if PLANTSMGR_MULTI_UPDATE
	LockListAccess();
#endif

	// shutdown the triangle entities
	for (s32 i=0; i<g_procInfo.m_procObjInfos.GetCount(); i++)
	{
		CEntityItem* pEntityItem = (CEntityItem*)g_procInfo.m_procObjInfos[i].m_EntityList.GetHead();

		while (pEntityItem)
		{
			CEntityItem* pNextEntityItem = (CEntityItem*)g_procInfo.m_procObjInfos[i].m_EntityList.GetNext(pEntityItem);

			g_procObjMan.RemoveObject(pEntityItem, &g_procInfo.m_procObjInfos[i].m_EntityList);

			pEntityItem = pNextEntityItem;
		}

		Assert(g_procInfo.m_procObjInfos[i].m_EntityList.GetNumItems()==0);
	}

	// shutdown the object entities
	CEntityItem* pEntityItem = (CEntityItem*)m_objectEntityList.GetHead();

	while (pEntityItem)
	{
		CEntityItem* pNextEntityItem = (CEntityItem*)m_objectEntityList.GetNext(pEntityItem);

		RemoveObject(pEntityItem, &m_objectEntityList);

		pEntityItem = pNextEntityItem;
	}

	Assert(m_objectEntityList.GetNumItems()==0);

	// remove entities from the pool
	Assert(m_entityPool.GetNumItems() == MAX_ENTITY_ITEMS);
	m_entityPool.RemoveAll();

	Assert(m_numAllocatedMatrices==0);

	m_objectsToCreate.Reset();
	m_trisToRemove.Reset();

#if PLANTSMGR_MULTI_UPDATE
	UnlockListAccess();
#endif
}


///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

void				CProcObjectMan::Update						()
{
#if __BANK
	if(fwTimer::IsGamePaused())
		return;

	m_bkNumActiveProcObjs = GetNumActiveObjects();
	if (m_numProcObjCObjects > m_bkMaxProcObjCObjects)
	{
		m_bkMaxProcObjCObjects = m_numProcObjCObjects;
	}
	
	m_bkNumProcObjsToAdd	= m_objectsToCreate.GetCount();
	m_bkNumProcObjsToAddSize= m_objectsToCreate.GetCapacity();

	m_bkNumTrisToRemove		= m_trisToRemove.GetCount();
	m_bkNumTrisToRemoveSize = m_trisToRemove.GetCapacity();

	if (m_bkNumActiveProcObjs==MAX_ENTITY_ITEMS && (fwTimer::GetSystemFrameCount()&63)==0)
	{
		Warningf("Too many procedural objects in this area\n");
	}

	if(m_printNumObjsSkipped && m_bkNumProcObjsSkipped)
	{
		Displayf("ProcObjects: %d were skipped this frame.", m_bkNumProcObjsSkipped);
	}
	m_bkNumProcObjsSkipped	= 0;	// reset stats
#endif // __BANK

	// go through the objects created by other objects to see if the parent has been deleted
	CEntityItem* pEntityItem = (CEntityItem*)m_objectEntityList.GetHead();

	while (pEntityItem)
	{
		CEntityItem* pNextEntityItem = (CEntityItem*)m_objectEntityList.GetNext(pEntityItem);

		if (pEntityItem->m_pParentEntity == NULL)
		{
			RemoveObject(pEntityItem, &m_objectEntityList);
		}

		pEntityItem = pNextEntityItem;
	}

	// get all of the insts within 30m of the camera
	if(!NetworkInterface::IsGameInProgress() && gPlantMgr.ProcObjCreationEnabled())
	{
		#define MAX_ENTITIES_TO_PROCESS (128)
		s32 numEntitiesToProcess = 0;
		CEntity* pEntityProcessList[MAX_ENTITIES_TO_PROCESS];
	#if ENABLE_PHYSICS_LOCK
		phIterator it(phIterator::PHITERATORLOCKTYPE_WRITELOCK);
	#else	// ENABLE_PHYSICS_LOCK
		phIterator it;
	#endif	// ENABLE_PHYSICS_LOCK
		it.InitCull_Sphere(camInterface::GetPos(), 30.0f);
		it.SetStateIncludeFlags(phLevelBase::STATE_FLAG_FIXED|phLevelBase::STATE_FLAG_ACTIVE|phLevelBase::STATE_FLAG_INACTIVE); // phLevelBase::STATE_FLAGS_ALL
		u16 curObjectLevelIndex = CPhysics::GetLevel()->GetFirstCulledObject(it);
		while (curObjectLevelIndex!=phInst::INVALID_INDEX && numEntitiesToProcess<MAX_ENTITIES_TO_PROCESS)
		{
			phInst& culledInst = *CPhysics::GetLevel()->GetInstance(curObjectLevelIndex);

			// get the entity from the inst
			CEntity* pEntity = CPhysics::GetEntityFromInst(&culledInst);
			//Assert(pEntity);
			if (pEntity && pEntity->IsArchetypeSet())
			{
				// check if this object has a procedural object 2d effect
				bool createsProcObjects = false;
				CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();

				GET_2D_EFFECTS_NOLIGHTS(pModelInfo);				
				for (s32 i=0; i<iNumEffects; i++)
				{
					C2dEffect* pEffect = (*pa2dEffects)[i];
					if (pEffect->GetType() == ET_PROCEDURALOBJECTS)
					{
						createsProcObjects = true;
						break;
					}
				}

				if (createsProcObjects)
				{
					// stop proc objs being created around broken pieces of fragments
					bool bSkipThisEntity = false;
					if(pEntity->GetIsTypeObject() && ((CObject*)pEntity)->GetCurrentPhysicsInst() && ((CObject*)pEntity)->GetCurrentPhysicsInst()->GetClassType()==PH_INST_FRAG_CACHE_OBJECT)
						bSkipThisEntity = true;

					// check if this entity has created objects already
					if (!pEntity->m_nFlags.bCreatedProcObjects && !bSkipThisEntity)
					{
						// object is in range and it hasn't created procedural objects yet - create some
						pEntityProcessList[numEntitiesToProcess] = pEntity;
						numEntitiesToProcess++;
					}
				}
			}

			curObjectLevelIndex = CPhysics::GetLevel()->GetNextCulledObject(it);
		}

		// process the entities
		for (s32 i=0; i<numEntitiesToProcess; i++)
		{
			ProcessEntityAdded(pEntityProcessList[i]);
		}
	}//if(!NetworkInterface::IsGameInProgress())...

	// go through the entity list to see what needs removed this frame
	pEntityItem = (CEntityItem*)m_objectEntityList.GetHead();
	while (pEntityItem)
	{
		CEntityItem* pNextEntityItem = (CEntityItem*)m_objectEntityList.GetNext(pEntityItem);

		Vector3 vCentre;
		float fRadius = pEntityItem->m_pParentEntity->GetBoundCentreAndRadius(vCentre);
		spdSphere camSphere(RCC_VEC3V(camInterface::GetPos()), ScalarV(30.0f));
		spdSphere parentEntitySphere(RCC_VEC3V(vCentre), ScalarV(fRadius));
		if (!camSphere.IntersectsSphere(camSphere))
		{
			RemoveObject(pEntityItem, &m_objectEntityList);
		}

		pEntityItem = pNextEntityItem;
	}

#if !PSN_PLANTSMGR_SPU_UPDATE
	CPlantMgrWrapper::UpdateEnd();	// 360 et al: wait for PlantsMgrUpdate task to finish before processing add and remove lists
#endif
	ProcessAddAndRemoveLists();
}

///////////////////////////////////////////////////////////////////////////////
//  ProcessTriangleRemoved
///////////////////////////////////////////////////////////////////////////////

void				CProcObjectMan::ProcessTriangleRemoved		(CPlantLocTri* pLocTri, s32 procTagId)
{
	Assert(pLocTri);

/*
	float height = 0.0f;
	if (CPlantLocTri::IsPtInTriangle2D(-219.7f, 940.7f, pLocTri->m_V1, pLocTri->m_V2, pLocTri->m_V3, Vector3(0.0f, 0.0f, 1.0f), &height))
	{
		Printf("jubhkljnb\n");
	}
*/

	// remove all objects associated with this triangle
	//s32 procTagId = PGTAMATERIALMGR->UnpackProcId(pLocTri->m_nSurfaceType);
	s32 procObjInfoIndex = g_procInfo.m_procTagTable[procTagId].procObjIndex;
	atHashValue currentProcTag = g_procInfo.m_procObjInfos[procObjInfoIndex].m_Tag;

	while (procObjInfoIndex<g_procInfo.m_procObjInfos.GetCount() && g_procInfo.m_procObjInfos[procObjInfoIndex].m_Tag == currentProcTag)
	{
		CEntityItem* pEntityItem = (CEntityItem*)g_procInfo.m_procObjInfos[procObjInfoIndex].m_EntityList.GetHead();

		while (pEntityItem)
		{
			CEntityItem* pNextEntityItem = (CEntityItem*)g_procInfo.m_procObjInfos[procObjInfoIndex].m_EntityList.GetNext(pEntityItem);

			// check if this entity was created on this triangle
			if (pEntityItem->m_pParentTri == pLocTri)
			{
				// Can't trust the triangle pointer anymore so set it to NULL
				pEntityItem->m_pParentTri = NULL;
				RemoveObject(pEntityItem, &g_procInfo.m_procObjInfos[procObjInfoIndex].m_EntityList);
			}

			pEntityItem = pNextEntityItem;
		}

		procObjInfoIndex++;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessEntityAdded
///////////////////////////////////////////////////////////////////////////////

s32				CProcObjectMan::ProcessEntityAdded			(CEntity* pEntity)
{
	Assert(pEntity);
	Assert(NetworkInterface::IsGameInProgress()==false);

#if __BANK
	if (m_disableEntityObjects)
	{
		return 0;
	}
#endif // __BANK

	s32 numObjectsAdded = 0;

	// go through the 2d effects looking for info about procedural object creation
	CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();

	GET_2D_EFFECTS_NOLIGHTS(pModelInfo);

	// pre process the data to find out how many objects we need to place for this object
	Assert(iNumEffects<128);
	s32 numObjsToCreate[128];

	// calc each segment info
	s32 currSegment = 0;
	Vector3 vecSegsMin, vecSegsMax;
	vecSegsMin.Zero();
	vecSegsMax.Zero();

	for (s32 i=0; i<iNumEffects; i++)
	{
		C2dEffect* pEffect = (*pa2dEffects)[i];
		if (pEffect->GetType() == ET_PROCEDURALOBJECTS)
		{
			CProcObjAttr* po = pEffect->GetProcObj();

			// calc how many objects to create
			float zoneArea = 2.0f * PI * po->radiusOuter;//(po->radiusOuter - po->radiusInner);
			numObjsToCreate[i] = static_cast<s32>(zoneArea * (1.0f/(po->spacing*po->spacing)));

			// check we have room to store them
			Assert((currSegment+numObjsToCreate[i]) < MAX_SEGMENTS);

			// calc the segment coords
			for (s32 j=0; j<numObjsToCreate[i]; j++)
			{
				// get a random angle and radius
				float angle = g_DrawRand.GetRanged(-PI, PI);
				float radius = g_DrawRand.GetRanged(po->radiusInner, po->radiusOuter);

				// set the drop position (A)
				Vector3 posA;
				pEffect->GetPos(posA);
				posA.x += rage::Sinf(angle)*radius;
				posA.y += rage::Cosf(angle)*radius;

				// set the dropped position (B)
				Vector3 posB = posA;
				posB.z -= 2.0f;

				// set the segment up
				m_segments[currSegment++].Set(posA, posB);

				if(posA.x > vecSegsMax.x)	vecSegsMax.x = posA.x;
				else if(posA.x < vecSegsMin.x)	vecSegsMin.x = posA.x;
				if(posA.y > vecSegsMax.y)	vecSegsMax.y = posA.y;
				else if(posA.y < vecSegsMin.y)	vecSegsMin.y = posA.y;
				if(posA.z > vecSegsMax.z)	vecSegsMax.z = posA.z;
				else if(posA.z < vecSegsMin.z)	vecSegsMin.z = posA.z;

				if(posB.x > vecSegsMax.x)	vecSegsMax.x = posB.x;
				else if(posB.x < vecSegsMin.x)	vecSegsMin.x = posB.x;
				if(posB.y > vecSegsMax.y)	vecSegsMax.y = posB.y;
				else if(posB.y < vecSegsMin.y)	vecSegsMin.y = posB.y;
				if(posB.z > vecSegsMax.z)	vecSegsMax.z = posB.z;
				else if(posB.z < vecSegsMin.z)	vecSegsMin.z = posB.z;
			}
		}
		else
		{
			numObjsToCreate[i] = 0;
		}
	}

	if(currSegment > 0)
	{
		Matrix34 m = MAT34V_TO_MATRIX34(pEntity->GetMatrix());
		for(int i=0; i<currSegment; i++)
		{
			m_pIntersections[i]->Reset();
			m_pSegments[i]->Transform(m);
		}

		// do the line tests on the segments to find the ground intersections
		Vector3 vecSegsCentre = 0.5f * (vecSegsMax + vecSegsMin);

		Matrix34 matSegBox = m;
		matSegBox.d = (vecSegsMax - vecSegsCentre) * 1.1f;

		vecSegsCentre = pEntity->TransformIntoWorldSpace(vecSegsCentre);

		// Fill in the descriptor for the batch test.
		WorldProbe::CShapeTestBatchDesc batchDesc;
		batchDesc.SetOptions(0);
		batchDesc.SetIncludeFlags(INCLUDE_FLAGS_ALL);
		batchDesc.SetTypeFlags(TYPE_FLAGS_ALL);
		batchDesc.SetExcludeTypeFlags(TYPE_FLAGS_NONE);
		ALLOC_AND_SET_PROBE_DESCRIPTORS(batchDesc,currSegment);

		WorldProbe::CShapeTestResults probeResultsStruct(m_intersections, MAX_SEGMENTS);
		for(int i = 0; i < currSegment; ++i)
		{
			WorldProbe::CShapeTestProbeDesc probeDesc;
			probeDesc.SetStartAndEnd(m_pSegments[i]->A, m_pSegments[i]->B);
			probeDesc.SetResultsStructure(&probeResultsStruct);
			probeDesc.SetFirstFreeResultOffset(i);
			probeDesc.SetMaxNumResultsToUse(1);
			batchDesc.AddLineTest(probeDesc);
		}

		// Specify a custom cull-volume for the batch test based on the spread of the bullets.
		WorldProbe::CCullVolumeBoxDesc cullVolDesc;
		cullVolDesc.SetBox(m, matSegBox.d);
		batchDesc.SetCustomCullVolume(&cullVolDesc);

		WorldProbe::GetShapeTestManager()->SubmitTest(batchDesc);
	}

	// add the actual objects
	s32 currIndex = 0;
	for (s32 i=0; i<iNumEffects; i++)
	{
		if (numObjsToCreate[i]>0)
		{
			numObjectsAdded += AddObjects(pEntity, (*pa2dEffects)[i], numObjsToCreate[i], m_pIntersections[currIndex]);
			currIndex += numObjsToCreate[i];
		}
	}

	pEntity->m_nFlags.bCreatedProcObjects = true;
	return numObjectsAdded;
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessEntityRemoved
///////////////////////////////////////////////////////////////////////////////

void				CProcObjectMan::ProcessEntityRemoved		(CEntity* pEntity)
{
	Assert(pEntity);

	// remove all objects associated with this object
	CEntityItem* pEntityItem = (CEntityItem*)m_objectEntityList.GetHead();

	while (pEntityItem)
	{
		CEntityItem* pNextEntityItem = (CEntityItem*)m_objectEntityList.GetNext(pEntityItem);

		// check if this entity was created on this triangle
		if (pEntityItem->m_pParentEntity == pEntity)
		{
			RemoveObject(pEntityItem, &m_objectEntityList);
		}

		pEntityItem = pNextEntityItem;
	}

	pEntity->m_nFlags.bCreatedProcObjects = false;
}



//
// name:		CProcObjectMan::AddObjectToAddList
// description:
void				CProcObjectMan::AddObjectToAddList			(Vector3::Param pos, Vector3::Param normal, CProcObjInfo* pProcObjInfo, CPlantLocTri* pLocTri)
{
#if PLANTSMGR_MULTI_UPDATE
	LockListAccess();
#endif

	if (m_objectsToCreate.GetCount()<MAX_ENTITY_ITEMS)
	{
		ProcObjectCreationInfo info;
		info.pos		= pos;
		info.normal		= normal;
#if __64BIT
		info.pProcObjInfo = pProcObjInfo;	
		info.pLocTri	= pLocTri;
#else
		info.pos.uw		= (u32)pProcObjInfo;	
		info.normal.uw	= (u32)pLocTri;
#endif
		m_objectsToCreate.Push(info);
	}

#if PLANTSMGR_MULTI_UPDATE
	UnlockListAccess();
#endif
}

//
// name:		CProcObjectMan::AddTriToRemoveList
// description:
void				CProcObjectMan::AddTriToRemoveList			(CPlantLocTri* pLocTri)
{
#if PLANTSMGR_MULTI_UPDATE
	LockListAccess();
#endif

	ProcTriRemovalInfo info;
	info.pLocTri = pLocTri;
	info.procTagId = PGTAMATERIALMGR->UnpackProcId(pLocTri->m_nSurfaceType);
#if PLANTSMGR_MULTI_UPDATE
	if(m_trisToRemove.GetCapacity())	// only add to remove list if Shutdown() wasn't called beforehand
	{
		if(CSystem::IsThisThreadId(SYS_THREAD_UPDATE))
		{
			// updating from main thread: reallocations are fine:
			m_trisToRemove.PushAndGrow(info, 64);
			Assert(m_trisToRemove.GetCapacity()<=7*MAX_ENTITY_ITEMS);
		}
		else
		{	// memory reallocation on update subtasks is prohibited (BS#1846753):
			Assertf(m_trisToRemove.GetCount() < m_trisToRemove.GetCapacity(), "trisToRemove is too small (%d)!", MAX_REMOVED_TRIS);
			if(m_trisToRemove.GetCount() < m_trisToRemove.GetCapacity())
			{
				m_trisToRemove.Push(info);
			}
		}
	}
#else
	m_trisToRemove.PushAndGrow(info);
	Assert(m_trisToRemove.GetCapacity()<=6*MAX_ENTITY_ITEMS);
#endif

#if PLANTSMGR_MULTI_UPDATE
	UnlockListAccess();
#endif
}

//
// name:		CProcObjectMan::ProcessAddAndRemovalLists
// description:	Process the list of entities to add and the list of triangles to remove
void				CProcObjectMan::ProcessAddAndRemoveLists	()
{
	LockListAccess();

	// Process remove list
	// Process this first as triangle may have been reused and we don't want to remove objects we have just generated
	for(s32 i=0; i<m_trisToRemove.GetCount(); i++)
	{
		ProcessTriangleRemoved(m_trisToRemove[i].pLocTri, m_trisToRemove[i].procTagId);
	}

	// Process add list
	for(s32 i=0; i<m_objectsToCreate.GetCount(); i++)
	{
		Vector3& pos				= m_objectsToCreate[i].pos;
		Vector3& normal				= m_objectsToCreate[i].normal;
#if __64BIT
		CProcObjInfo* pProcObjInfo	= m_objectsToCreate[i].pProcObjInfo;
		CPlantLocTri* pLocTri		= m_objectsToCreate[i].pLocTri;
#else
		CProcObjInfo* pProcObjInfo	= (CProcObjInfo*)pos.uw;
		CPlantLocTri* pLocTri		= (CPlantLocTri*)normal.uw;
#endif

		CEntityItem* pEntityItem = AddObject(pos, normal, pProcObjInfo, &pProcObjInfo->m_EntityList, const_cast<CEntity*>(pLocTri->m_pParentEntity), pLocTri->GetSeed());
		if (pEntityItem)
		{
			pEntityItem->m_pParentTri = pLocTri;

			if(pEntityItem->m_pParentTri)
			{
				pEntityItem->m_pData->SetNaturalAmbientScale((u32)pEntityItem->m_pParentTri->m_nAmbientScale[0]);
				pEntityItem->m_pData->SetArtificialAmbientScale((u32)pEntityItem->m_pParentTri->m_nAmbientScale[1]);
			}

#if __BANK
			pEntityItem->triVert1[0] = pLocTri->GetV1().x;
			pEntityItem->triVert1[1] = pLocTri->GetV1().y;
			pEntityItem->triVert1[2] = pLocTri->GetV1().z;

			pEntityItem->triVert2[0] = pLocTri->GetV2().x;
			pEntityItem->triVert2[1] = pLocTri->GetV2().y;
			pEntityItem->triVert2[2] = pLocTri->GetV2().z;

			pEntityItem->triVert3[0] = pLocTri->GetV3().x;
			pEntityItem->triVert3[1] = pLocTri->GetV3().y;
			pEntityItem->triVert3[2] = pLocTri->GetV3().z;
#endif // __BANK
		}
		else
		{
		#if __BANK
			m_bkNumProcObjsSkipped++;
		#endif
		}
	}

	// reset lists
	m_objectsToCreate.ResetCount();
	m_trisToRemove.ResetCount();

	UnlockListAccess();
}

void				CProcObjectMan::LockListAccess ()
{
	listAccessToken.Lock();
}
void				CProcObjectMan::UnlockListAccess ()
{
	listAccessToken.Unlock();
}

///////////////////////////////////////////////////////////////////////////////
//  AddObjects
///////////////////////////////////////////////////////////////////////////////

s32				CProcObjectMan::AddObjects					(CEntity* pEntity, C2dEffect* p2dEffect, s32 numObjsToCreate, phIntersection* pIntersections)
{
	// set up the data
	CProcObjInfo procObjInfo;
	CProcObjAttr* po = p2dEffect->GetProcObj();

	fwModelId modelId;
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey(po->objHash, &modelId);

	if (!Verifyf(pModelInfo!=NULL, "Cannot find procedural object attached to %s.", pEntity->GetModelName()))
	{
		return 0;
	}

	// we don't want to add tree objects in this way - make sure we aren't trying to
	if (pModelInfo->GetModelType()!=MI_TYPE_BASE)
	{
		Warningf("Trying to add a procedural tree object on another object\n");
		Assert(0);
	}

	procObjInfo.m_ModelIndex = modelId.GetModelIndex();

	procObjInfo.m_Spacing.SetFloat16_FromFloat32(po->spacing);
	procObjInfo.m_MinXRotation.SetFloat16_FromFloat32(0.0f);
	procObjInfo.m_MaxXRotation.SetFloat16_FromFloat32(0.0f);
	procObjInfo.m_MinYRotation.SetFloat16_FromFloat32(0.0f);
	procObjInfo.m_MaxYRotation.SetFloat16_FromFloat32(0.0f);
	procObjInfo.m_MinZRotation.SetFloat16_FromFloat32(-180.0f);
	procObjInfo.m_MaxZRotation.SetFloat16_FromFloat32(180.0f);
	procObjInfo.m_MinScale.SetFloat16_FromFloat32(po->minScale);
	procObjInfo.m_MaxScale.SetFloat16_FromFloat32(po->maxScale);
	procObjInfo.m_MinScaleZ.SetFloat16_FromFloat32(po->minScaleZ);
	procObjInfo.m_MaxScaleZ.SetFloat16_FromFloat32(po->maxScaleZ);
	procObjInfo.m_MinZOffset.SetFloat16_FromFloat32(po->zOffsetMin);
	procObjInfo.m_MaxZOffset.SetFloat16_FromFloat32(po->zOffsetMax);
	if(po->isAligned)
		procObjInfo.m_Flags.Set(PROCOBJ_ALIGN_OBJ);

	// temp hack to half the density as artists are going wild with these!
	procObjInfo.m_Spacing.SetFloat16_FromFloat32(procObjInfo.m_Spacing.GetFloat32_FromFloat16() * 1.141421356f);

	procObjInfo.m_Tag.Clear();			// mark temp procObjInfo in a special way
	Assert(procObjInfo.m_Tag.IsNull());

	// create the objects
	s32 numObjectsAdded = 0;

	for (s32 i=0; i<numObjsToCreate; i++)
	{
		if (pIntersections[i].GetInstance())
		{
			const Vector3 seedPos(RCC_VECTOR3(pIntersections[i].GetPosition()));
			const s32 seed = seedPos.ux ^ (seedPos.uy | seedPos.uz);
			CEntityItem* pEntityItem = g_procObjMan.AddObject(RCC_VECTOR3(pIntersections[i].GetPosition()), RCC_VECTOR3(pIntersections[i].GetNormal()), &procObjInfo, &m_objectEntityList, pEntity, seed);

			if (pEntityItem)
			{
				numObjectsAdded++;
				pEntityItem->m_pParentEntity = pEntity;
			#if __BANK
				pEntityItem->triVert1[0] = 0.0f;
				pEntityItem->triVert1[1] = 0.0f;
				pEntityItem->triVert1[2] = 0.0f;

				pEntityItem->triVert2[0] = 0.0f;
				pEntityItem->triVert2[1] = 0.0f;
				pEntityItem->triVert2[2] = 0.0f;

				pEntityItem->triVert3[0] = 0.0f;
				pEntityItem->triVert3[1] = 0.0f;
				pEntityItem->triVert3[2] = 0.0f;
			#endif // __BANK

			#if __DEV
				pEntityItem->m_pData->m_nFlags.bIsEntityProcObject = true;
			#endif //  __DEV
			}
			else
			{
			#if __BANK
				m_bkNumProcObjsSkipped++;
			#endif
			}
		}
	}

	return numObjectsAdded;
}


///////////////////////////////////////////////////////////////////////////////
//  AddObject
///////////////////////////////////////////////////////////////////////////////

CEntityItem*		CProcObjectMan::AddObject					(const Vector3& pos, const Vector3& normal, CProcObjInfo* pProcObjData, vfxList* pList, CEntity* pParentEntity, s32 seed)
{
	#define NORMAL_Z_THRESH (0.97f)

	if(pProcObjData->m_Flags.IsClear(PROCOBJ_NETWORK_GAME) && NetworkInterface::IsGameInProgress())	// allow only objects flagged for network games
	{
		return NULL;
	}

	// try to get an object from the pool
	CEntityItem* pEntityItem = (CEntityItem*)g_procObjMan.GetEntityFromPool();
	if (pEntityItem)
	{
		Assert(pEntityItem->m_pData==NULL);

		// if it needs aligned then check we have a spare matrix
		if (pProcObjData->m_Flags.IsSet(PROCOBJ_ALIGN_OBJ) && normal.z<NORMAL_Z_THRESH)
		{
 			if (g_procObjMan.m_numAllocatedMatrices>=MAX_ALLOCATED_MATRICES)
 			{
 				// no spare matrices - don't create
// 				Warningf("Cannot create procedural object aligned to triangle - no matrices left\n");
				g_procObjMan.ReturnEntityToPool(pEntityItem);
 				return NULL;
 			}
		}

		// we've got one - create the entity
		fwModelId modelId(pProcObjData->m_ModelIndex);
		CBaseModelInfo* pBaseModelInfo = CModelInfo::GetBaseModelInfo(modelId);
		if (pBaseModelInfo==NULL)
		{
			g_procObjMan.ReturnEntityToPool(pEntityItem);
			return NULL;
		}

		const bool isTypeObject = pBaseModelInfo->GetIsTypeObject();

		if (pBaseModelInfo->GetModelType()==MI_TYPE_BASE)
		{
			if (isTypeObject)
			{
				// If we only have 50 objects left then don't create procedural objects
				if(CObject::GetPool()->GetNoOfFreeSpaces() < 50)
				{
					g_procObjMan.ReturnEntityToPool(pEntityItem);
					return NULL;
				}

				if (m_numProcObjCObjects >= MAX_PROC_COBJECTS)
				{
					g_procObjMan.ReturnEntityToPool(pEntityItem);
					return NULL;
				}
				// it's a game object - create as object and set up object specific data
	//			CPools::GetObjectPool().SetCanDealWithNoMemory(true);
				fwModelId modelId(pProcObjData->m_ModelIndex);
				pEntityItem->m_pData = CObjectPopulation::CreateObject(modelId, ENTITY_OWNEDBY_PROCEDURAL, false);
				if (pEntityItem->m_pData == NULL)
				{
	//				CPools::GetObjectPool().SetCanDealWithNoMemory(false);
					g_procObjMan.ReturnEntityToPool(pEntityItem);
					return NULL;
				}
//				((CObject*)pEntityItem->m_pData)->GetOwnedBy() = ENTITY_OWNEDBY_GAME;	Now done in CObjectPopulation::CreateObject
	//			((CObject*)pEntityItem->m_pData)->SetMoveSpeed(0.0f, 0.0f, 0.0f);
	//			((CObject*)pEntityItem->m_pData)->m_lightingFromCollision = lighting/255.0f;
	//			CPools::GetObjectPool().SetCanDealWithNoMemory(false);

				m_numProcObjCObjects++;

				CObject *pObject = (CObject*)pEntityItem->m_pData;
				if(pProcObjData->m_Tag.IsNotNull())
				{
					const u32 procObjInfoIdx = g_procInfo.GetProcObjInfoIdx(pProcObjData);
					Assert(procObjInfoIdx <= 0xfffe);	// must fit into u16
					pObject->m_procObjInfoIdx = (u16)procObjInfoIdx;
				}
				else
				{
					pObject->m_procObjInfoIdx = 0xffff;	// mark as invalid (e.g. procObjInfo is temp created for 2deffect)
				}
			}
			else
			{
				// If we only have 500 buildings left then don't create procedural objects
				if(CBuilding::GetPool()->GetNoOfFreeSpaces() < 500)
				{
					g_procObjMan.ReturnEntityToPool(pEntityItem);
					return NULL;
				}
				// it's not a game object - create as building and set up building specific data
	//			CPools::GetBuildingPool().SetCanDealWithNoMemory(true);
				pEntityItem->m_pData = rage_new CBuilding( ENTITY_OWNEDBY_PROCEDURAL );
				if (pEntityItem->m_pData == NULL)
				{
	//				CPools::GetBuildingPool().SetCanDealWithNoMemory(false);
					g_procObjMan.ReturnEntityToPool(pEntityItem);
					return NULL;
				}

				fwModelId modelId(pProcObjData->m_ModelIndex);
				pEntityItem->m_pData->SetModelId(modelId);

	//			CPools::GetBuildingPool().SetCanDealWithNoMemory(false);

				m_numProcObjCBuildings++;
			}
		}
/*		else if (pBaseModelInfo->GetModelType()==MI_TYPE_TREE)
		{
			Assert(isTypeObject==false);

			pEntityItem->m_pData = rage_new CTree();
			if (pEntityItem->m_pData == NULL)
			{
				g_procObjMan.ReturnEntityToPool(pEntityItem);
				return NULL;
			}
			pEntityItem->m_pData->SetModel Index(pProcObjData->modelIndex);
		}*/

//		// removed for now - can add later again if required
//		if (PGTAMATERIALMGR->GetIsWater(m_materialId))
//		{
//			pEntityItem->m_pData->m_nFlags.bUnderwater = true;
//		}

		// check that we've create the object ok
		if (pEntityItem->m_pData==NULL)
		{
			Assertf(0, "Cannot create procedural object\n");
			g_procObjMan.ReturnEntityToPool(pEntityItem);
			return NULL;
		}

		pEntityItem->m_pData->m_nFlags.bIsProcObject = true;
		if (pProcObjData->m_Flags.IsSet(PROCOBJ_IS_FLOATING))
		{
			pEntityItem->m_pData->m_nFlags.bIsFloatingProcObject = true;
		}
		// procedural objects are considered unimportant when streaming
		pEntityItem->m_pData->SetBaseFlag(fwEntity::LOW_PRIORITY_STREAM);

		// Force ambient scale use
		modelId.SetModelIndex(pProcObjData->m_ModelIndex.Get());
		CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(modelId);
		pModelInfo->SetUseAmbientScale(true);

		s32 prevSeed=0;
		if(pProcObjData->m_Flags.IsSet(PROCOBJ_USE_SEED))
		{
			prevSeed = g_DrawRand.GetSeed();
			g_DrawRand.Reset((seed ^ (pProcObjData->m_ModelName.GetHash() >> (seed&0x000f))));
		}

		// calculate the random info
		float scale = g_DrawRand.GetRanged(pProcObjData->m_MinScale.GetFloat32_FromFloat16(), pProcObjData->m_MaxScale.GetFloat32_FromFloat16());
		const float minScaleZ = pProcObjData->m_MinScaleZ.GetFloat32_FromFloat16();
		const float maxScaleZ = pProcObjData->m_MaxScaleZ.GetFloat32_FromFloat16();
		float scaleZ = (minScaleZ<0.0f) ? scale : g_DrawRand.GetRanged(minScaleZ, maxScaleZ);
		float xRot = g_DrawRand.GetRanged(pProcObjData->m_MinXRotation.GetFloat32_FromFloat16(), pProcObjData->m_MaxXRotation.GetFloat32_FromFloat16());
		float yRot = g_DrawRand.GetRanged(pProcObjData->m_MinYRotation.GetFloat32_FromFloat16(), pProcObjData->m_MaxYRotation.GetFloat32_FromFloat16());
		float zRot = g_DrawRand.GetRanged(pProcObjData->m_MinZRotation.GetFloat32_FromFloat16(), pProcObjData->m_MaxZRotation.GetFloat32_FromFloat16());
		float zOff = g_DrawRand.GetRanged(pProcObjData->m_MinZOffset.GetFloat32_FromFloat16(), pProcObjData->m_MaxZOffset.GetFloat32_FromFloat16());

		if(pProcObjData->m_Flags.IsSet(PROCOBJ_USE_SEED))
		{
			g_DrawRand.Reset(prevSeed);
		}

		// calculate the z pos
		Vector3 bbMin = pModelInfo->GetBoundingBoxMin();
		if (pProcObjData->m_Flags.IsSet(PROCOBJ_ALIGN_OBJ) && normal.z<NORMAL_Z_THRESH)
		{
			Assert(g_procObjMan.m_numAllocatedMatrices<MAX_ALLOCATED_MATRICES);

			// this object wants aligned to the normal
			Matrix34 objMat;
			Vector3 xVec;
			xVec.x = rage::Cosf(zRot);
			xVec.y = rage::Sinf(zRot);
			xVec.z = 0.0f;
			Vector3 yAxis;
			yAxis.Cross(normal, xVec);
			yAxis.Normalize();
			Vector3 xAxis;
			xAxis.Cross(yAxis, normal);
			xAxis.Normalize();

			objMat.a = xAxis;
			objMat.b = yAxis;
			objMat.c = normal;
			objMat.d = pos + (normal*(zOff-bbMin.z));

		#if ENABLE_MATRIX_MEMBER
			pEntityItem->m_pData->SetTransform(RC_MAT34V(objMat));
		#else
			pEntityItem->m_pData->SetTransform(rage_new fwMatrixScaledTransform(RC_MAT34V(objMat)));
		#endif
			((fwMatrixScaledTransform*)pEntityItem->m_pData->GetTransformPtr())->SetScale(1.0f, 1.0f);

			pEntityItem->m_allocatedMatrix = true;
			g_procObjMan.m_numAllocatedMatrices++;
		}
		else if (xRot>0.0f || yRot>0.0f)
		{
			Mat34V objMat;
			Mat34VFromEulersXYZ(objMat, Vec3V(xRot, yRot, zRot), Vec3V(pos.x, pos.y, pos.z - bbMin.z + zOff));
		#if ENABLE_MATRIX_MEMBER
			pEntityItem->m_pData->SetTransform(objMat);
		#else
			pEntityItem->m_pData->SetTransform(rage_new fwMatrixScaledTransform(objMat));
		#endif
			((fwMatrixScaledTransform*)pEntityItem->m_pData->GetTransformPtr())->SetScale(1.0f, 1.0f);

			pEntityItem->m_allocatedMatrix = true;
			g_procObjMan.m_numAllocatedMatrices++;
		}
		else
		{
			if(isTypeObject)
			{
				Assert(pEntityItem->m_pData->GetBaseFlags()&fwEntity::IS_DYNAMIC);
				// CPhysicals require full matrix transform:
				Mat34V objMat;
				Mat34VFromZAxisAngle(objMat, ScalarV(zRot), Vec3V(pos.x, pos.y, pos.z - bbMin.z + zOff));
			#if ENABLE_MATRIX_MEMBER
				pEntityItem->m_pData->SetTransform( objMat );
			#else
				pEntityItem->m_pData->SetTransform( rage_new fwMatrixScaledTransform(objMat) );
			#endif
				((fwMatrixScaledTransform*)pEntityItem->m_pData->GetTransformPtr())->SetScale(1.0f, 1.0f);

				pEntityItem->m_allocatedMatrix = true;
				g_procObjMan.m_numAllocatedMatrices++;
			}
			else
			{
			#if ENABLE_MATRIX_MEMBER
				fwTransform *tmp = rage_new fwSimpleTransform(pos.x, pos.y, pos.z - bbMin.z + zOff, zRot);
				pEntityItem->m_pData->SetTransform(tmp->GetMatrix());
				pEntityItem->m_pData->SetTransformScale(1.0f, 1.0f);
				tmp->Delete();
			#else
				pEntityItem->m_pData->SetTransform(rage_new fwSimpleTransform(pos.x, pos.y, pos.z - bbMin.z + zOff, zRot));
			#endif
				pEntityItem->m_allocatedMatrix = false;
			}
		}

		// scale static objects if required
		if((scale!=1.0f) || (scaleZ!=1.0f))
		{
			if(true) //if(Verifyf(isTypeObject==false, "WARNING: Trying to scale a procedurally generated dynamic game object %s in proc type %s", pBaseModelInfo->GetModelName(), pProcObjData->m_PlantTag.GetCStr()))
			{
				if (pEntityItem->m_allocatedMatrix)
				{
					Assert(pEntityItem->m_pData->GetTransform().IsMatrixScaledTransform());
					fwMatrixScaledTransform *matTransform = (fwMatrixScaledTransform*)&pEntityItem->m_pData->GetTransform();
					matTransform->SetScale(scale, scaleZ);
				}
				else if (g_procObjMan.m_numAllocatedMatrices<MAX_ALLOCATED_MATRICES)
				{
					Assert(pEntityItem->m_allocatedMatrix==false);
					// we need to allocate this matrix if it hasn't been already
					Matrix34 objMat = MAT34V_TO_MATRIX34(pEntityItem->m_pData->GetMatrix());
					float zOffset = (bbMin.z*scaleZ) - bbMin.z;
					objMat.Translate(0.0f, 0.0f, -zOffset);

				#if ENABLE_MATRIX_MEMBER
					Mat34V trans = RC_MAT34V(objMat);					
					pEntityItem->m_pData->SetTransform(trans);
					pEntityItem->m_pData->SetTransformScale(scale, scaleZ);
				#else
					fwTransform* trans = rage_new fwMatrixScaledTransform(RC_MAT34V(objMat));
					trans->SetScale(scale, scaleZ);
					pEntityItem->m_pData->SetTransform(trans);
				#endif

					pEntityItem->m_allocatedMatrix = true;
					g_procObjMan.m_numAllocatedMatrices++;
				}
			}
		}

		// Now that we've set the position on the entity, request a physics update so that the physics bounds match up. 
		// Otherwise, the entity desc (and possibly other stuff) will get updated incorrectly.
		pEntityItem->m_pData->RequestUpdateInPhysics();

		if(pProcObjData->m_Flags.IsSet(PROCOBJ_CAST_SHADOW))
			pEntityItem->m_pData->GetRenderPhaseVisibilityMask().SetFlag( VIS_PHASE_MASK_SHADOWS );
		else
			pEntityItem->m_pData->GetRenderPhaseVisibilityMask().ClearFlag( VIS_PHASE_MASK_SHADOWS );

		if(pEntityItem->m_pData->GetBoundRadius() >= g_procObjMan.m_fMinRadiusForInclusionInHeightMap)
			pEntityItem->m_pData->GetRenderPhaseVisibilityMask().SetFlag( VIS_PHASE_MASK_RAIN_COLLISION_MAP );
		else
			pEntityItem->m_pData->GetRenderPhaseVisibilityMask().ClearFlag( VIS_PHASE_MASK_RAIN_COLLISION_MAP );

		pEntityItem->m_pData->SetBaseFlag(fwEntity::USE_SCREENDOOR);

		fwInteriorLocation	interiorLocation;

		// add to the world and to our list
		if (pParentEntity && pParentEntity->IsArchetypeSet())
		{
			interiorLocation = pParentEntity->GetInteriorLocation();
		}
		else 
		{
			interiorLocation.MakeInvalid();
		}
		CGameWorld::Add(pEntityItem->m_pData, interiorLocation);
		
		
		/*
		if(pEntityItem->m_pData->m_nFlags.physicsEntity==TRUE)
		{
			............
		}
		*/


		pList->AddItem(pEntityItem);

		return pEntityItem;
	}

	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
//  RemoveObject
///////////////////////////////////////////////////////////////////////////////

void				CProcObjectMan::RemoveObject				(CEntityItem* pEntityItem, vfxList* pList)
{
#if __DEV
	m_removingProcObject = true;
#endif
	Assert(pEntityItem->m_pData);

	if (pEntityItem->m_pParentEntity)
	{
		pEntityItem->m_pParentEntity->m_nFlags.bCreatedProcObjects = false;
	}

	if (pEntityItem->m_pData->GetIsTypeBuilding())
	{
		m_numProcObjCBuildings--;
	}
	else
	{
		Assert(pEntityItem->m_pData->GetIsTypeObject());
		m_numProcObjCObjects--;
	}

	CGameWorld::Remove(pEntityItem->m_pData);

#if __DEV
	CObject::bDeletingProcObject = true;
#endif

	REPLAY_ONLY(CReplayMgr::OnDelete(pEntityItem->m_pData));

	delete pEntityItem->m_pData;

#if __DEV
	CObject::bDeletingProcObject = false;
#endif

	pEntityItem->m_pData = NULL;

	if (pEntityItem->m_allocatedMatrix)
	{
		m_numAllocatedMatrices--;
		pEntityItem->m_allocatedMatrix = false;
	}

#if __ASSERT
	u32 numItemsInList = pList->GetNumItems();
#endif
	pList->RemoveItem(pEntityItem);
	Assert(numItemsInList-1 == pList->GetNumItems());

	g_procObjMan.ReturnEntityToPool(pEntityItem);

#if __DEV
	m_removingProcObject = false;
#endif
}


///////////////////////////////////////////////////////////////////////////////
//  GetEntityFromPool
///////////////////////////////////////////////////////////////////////////////

CEntityItem*		CProcObjectMan::GetEntityFromPool			()
{
	CEntityItem* pEntity = (CEntityItem*)m_entityPool.RemoveHead();

	if (pEntity)
	{
		Assert(pEntity->m_pData == NULL);
		Assert(pEntity->m_pParentTri == NULL);
		Assert(pEntity->m_pParentEntity == NULL);
		Assert(pEntity->m_allocatedMatrix == false);
	}

	return pEntity;
}


///////////////////////////////////////////////////////////////////////////////
//  ReturnEntityToPool
///////////////////////////////////////////////////////////////////////////////

void				CProcObjectMan::ReturnEntityToPool			(CEntityItem* pEntityItem)
{
	pEntityItem->m_pData = NULL;
	pEntityItem->m_pParentTri = NULL;
	pEntityItem->m_pParentEntity = NULL;
	m_entityPool.AddItem(pEntityItem);
}

///////////////////////////////////////////////////////////////////////////////
//  GetNumActiveObjects
///////////////////////////////////////////////////////////////////////////////

s32				CProcObjectMan::GetNumActiveObjects			()
{
	return ( MAX_ENTITY_ITEMS - this->m_entityPool.GetNumItems() );
}

///////////////////////////////////////////////////////////////////////////////
//  UpdateVisualDataSettings
///////////////////////////////////////////////////////////////////////////////

void			CProcObjectMan::UpdateVisualDataSettings	()
{
	m_fMinRadiusForInclusionInHeightMap = g_visualSettings.Get( "procObjects.minRadiusForHeightMap",
		DEFAULT_MIN_RADIUS_FOR_INCLUSION_IN_HEIGHT_MAP);
}


#if __DEV

///////////////////////////////////////////////////////////////////////////////
//  IsAllowedToDelete
///////////////////////////////////////////////////////////////////////////////

bool				CProcObjectMan::IsAllowedToDelete()
{
	return m_removingProcObject;
}

#endif





#if __BANK

///////////////////////////////////////////////////////////////////////////////
//  InitWidgets
///////////////////////////////////////////////////////////////////////////////

void				CProcObjectMan::InitWidgets				()
{
	char txt[128];

	bkBank* pBank = BANKMGR.FindBank("Objects");
	if (pBank)
	{
		pBank->PushGroup("Procedural Objects", false);
		{
			pBank->AddToggle("Disable Entity Objects ", &m_disableEntityObjects);
			pBank->AddToggle("Disable Collision Objects ", &m_disableCollisionObjects);
			pBank->AddToggle("Render Debug Polys", &m_renderDebugPolys);
			pBank->AddToggle("Render Debug Zones", &m_renderDebugZones);
			pBank->AddToggle("Ignore Min Dist", &m_ignoreMinDist);
			pBank->AddToggle("Ignore Seeding", &m_ignoreSeeding);
			pBank->AddToggle("Enable Seeding", &m_enableSeeding);
			pBank->AddToggle("Force One Obj Per Tri", &m_forceOneObjPerTri);
			pBank->AddToggle("Print Num Skipped", &m_printNumObjsSkipped);

			sprintf(txt, "Active Objects (%d)", MAX_ENTITY_ITEMS);
			pBank->AddSlider(txt, &m_bkNumActiveProcObjs, 0, MAX_ENTITY_ITEMS, 0);

			sprintf(txt, "Num CBuildings (%d)", MAX_PROC_CBUILDINGS);
			pBank->AddSlider(txt, &m_numProcObjCBuildings, 0, MAX_PROC_CBUILDINGS, 0);

			sprintf(txt, "Num CObjects (%d)", MAX_PROC_COBJECTS);
			pBank->AddSlider(txt, &m_numProcObjCObjects, 0, MAX_PROC_COBJECTS, 0);

			pBank->AddSlider("Max CObjects", &m_bkMaxProcObjCObjects, 0, MAX_ENTITY_ITEMS, 0);
			pBank->AddSlider("Num To Add", &m_bkNumProcObjsToAdd, 0, MAX_ENTITY_ITEMS, 0);
			pBank->AddSlider("Num To Add Size", &m_bkNumProcObjsToAddSize, 0, MAX_ENTITY_ITEMS, 0);
			pBank->AddSlider("Num Skipped", &m_bkNumProcObjsSkipped, 0, MAX_ENTITY_ITEMS*2, 0);
			pBank->AddSlider("Num To Remove", &m_bkNumTrisToRemove, 0, MAX_ENTITY_ITEMS, 0);
			pBank->AddSlider("Num To Remove Size", &m_bkNumTrisToRemoveSize, 0, MAX_ENTITY_ITEMS, 0);
			sprintf(txt, "Allocated Matrices (%d)", MAX_ALLOCATED_MATRICES);
			pBank->AddSlider(txt, &m_numAllocatedMatrices, 0, MAX_ALLOCATED_MATRICES, 0);
			extern void ReloadProceduralDotDatCB();
			pBank->AddButton("Reload Data File", ReloadProceduralDotDatCB);
		}
		pBank->PopGroup();
	}
}
	
///////////////////////////////////////////////////////////////////////////////
//  RenderDebug
///////////////////////////////////////////////////////////////////////////////

void				CProcObjectMan::RenderDebug				()
{
	if (m_renderDebugPolys)
	{
		for (s32 i=0; i<g_procInfo.m_procObjInfos.GetCount(); i++)
		{
			grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
			grcStateBlock::SetDepthStencilState(CShaderLib::DSS_Default_Invert);
			grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);

			CEntityItem* pEntityItem = (CEntityItem*)g_procInfo.m_procObjInfos[i].m_EntityList.GetHead();

			while (pEntityItem)
			{
				if( pEntityItem->m_pData )
				{
					grcDebugDraw::Axis(MAT34V_TO_MATRIX34(pEntityItem->m_pData->GetMatrix()),0.5f);
				}

				if( pEntityItem->m_pParentEntity )
				{
					grcDebugDraw::Axis(MAT34V_TO_MATRIX34(pEntityItem->m_pParentEntity->GetMatrix()),1.0f);
				}

				grcBegin(drawTris, 3);

				float seed = pEntityItem->triVert1[0] + pEntityItem->triVert1[1] + pEntityItem->triVert1[2] +
							 pEntityItem->triVert2[0] + pEntityItem->triVert2[1] + pEntityItem->triVert2[2] +
							 pEntityItem->triVert3[0] + pEntityItem->triVert3[1] + pEntityItem->triVert3[2];
				seed -= static_cast<s32>(seed);

				u8 col = static_cast<u8>(255*seed);

				grcColor(Color32(col, col, 128, 255));

				grcVertex3f(pEntityItem->triVert1[0], pEntityItem->triVert1[1], pEntityItem->triVert1[2] + 0.05f);
				grcVertex3f(pEntityItem->triVert2[0], pEntityItem->triVert2[1], pEntityItem->triVert2[2] + 0.05f);
				grcVertex3f(pEntityItem->triVert3[0], pEntityItem->triVert3[1], pEntityItem->triVert3[2] + 0.05f);

				grcEnd();

				pEntityItem = (CEntityItem*)g_procInfo.m_procObjInfos[i].m_EntityList.GetNext(pEntityItem);
			}
		}
	}

	// render debug entity zone
	if (m_renderDebugZones)
	{
		RenderDebugEntityZones();
	}

	// render debug segments
//	Color32 col(1.0f, 1.0f, 0.0f, 1.0f);
//	for (s32 i=0; i<MAX_SEGMENTS; i++)
//	{
//		grcDebugDraw::Line(m_segments[i].A, m_segments[i].B, col, col);
//	}
}


///////////////////////////////////////////////////////////////////////////////
//  RenderDebugEntityZones
///////////////////////////////////////////////////////////////////////////////

void				CProcObjectMan::RenderDebugEntityZones		()
{
	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetDepthStencilState(CShaderLib::DSS_Default_Invert);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);
	grcWorldIdentity();
	grcBindTexture(NULL);

	// get all of the insts within 15m of the camera
#if ENABLE_PHYSICS_LOCK
	phIterator it(phIterator::PHITERATORLOCKTYPE_READLOCK);
#else	// ENABLE_PHYSICS_LOCK
	phIterator it;
#endif	// ENABLE_PHYSICS_LOCK
	it.InitCull_Sphere(camInterface::GetPos(), 15.0f);
	it.SetStateIncludeFlags(phLevelBase::STATE_FLAG_FIXED|phLevelBase::STATE_FLAG_ACTIVE|phLevelBase::STATE_FLAG_INACTIVE); // phLevelBase::STATE_FLAGS_ALL
	u16 curObjectLevelIndex = CPhysics::GetLevel()->GetFirstCulledObject(it);
	while (curObjectLevelIndex != phInst::INVALID_INDEX)
	{
		phInst& culledInst = *CPhysics::GetLevel()->GetInstance(curObjectLevelIndex);

		// get the entity from the inst
		CEntity* pEntity = CPhysics::GetEntityFromInst(&culledInst);
		Assert(pEntity);
		if (pEntity && pEntity->IsArchetypeSet())
		{
			// check if this object has a procedural object 2d effect
			CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();

			GET_2D_EFFECTS_NOLIGHTS(pModelInfo);
			for (s32 i=0; i<iNumEffects; i++)
			{
				C2dEffect* pEffect = (*pa2dEffects)[i];
				if (pEffect->GetType() == ET_PROCEDURALOBJECTS)
				{
					CProcObjAttr* po = pEffect->GetProcObj();

					Vector3 PosSrc;
					pEffect->GetPos(PosSrc);
					Vector3 pos;// = pEntity->TransformIntoWorldSpace(PosSrc);
					pEntity->TransformIntoWorldSpace(pos, PosSrc);

					grcColor(Color32(255, 255, 0, 255));
					grcDrawCircle(po->radiusOuter, pos, XAXIS, YAXIS, 20, false, true);

					pos.z += 0.02f;
					grcColor(Color32(255, 0, 0, 255));
					grcDrawCircle(po->radiusInner, pos, XAXIS, YAXIS, 20, false, true);
				}
			}
		}

		curObjectLevelIndex = CPhysics::GetLevel()->GetNextCulledObject(it);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ReloadData
///////////////////////////////////////////////////////////////////////////////

void				CProcObjectMan::ReloadData					()
{
#if 0 // no more necessary to re-add tris - they will be readded by PlantsMgr::Update():
	// create a list of any triangles currently used
	#define MAX_STORED_TRIS (1024)
	s32 numStoredTris = 0;
	CPlantLocTri* pStoredTris[MAX_STORED_TRIS];

	for (s32 i=0; i<g_procInfo.m_numProcObjInfos; i++)
	{
		CEntityItem* pEntityItem = (CEntityItem*)g_procInfo.m_procObjInfos[i].m_entityList.GetHead();

		while (pEntityItem)
		{
			// search for this tri in the list already
			bool foundInList = false;
			for (s32 j=0; j<numStoredTris; j++)
			{
				if (pEntityItem->m_pParentTri == pStoredTris[j])
				{
					foundInList = true;
					break;
				}
			}

			// add to the list if it wasn't in there already
			if (foundInList==false)
			{
				Assert(numStoredTris<MAX_STORED_TRIS);
				pStoredTris[numStoredTris++] = pEntityItem->m_pParentTri;
			}

			pEntityItem = (CEntityItem*)g_procInfo.m_procObjInfos[i].m_entityList.GetNext(pEntityItem);
		}
	}
#endif //#if 0...

	g_procObjMan.Shutdown();
	g_procObjMan.Init();
	g_procInfo.Init();

#if 0
	// restore any triangles that were populated previously
	for (s32 i=0; i<numStoredTris; i++)
	{
		g_procObjMan.ProcessTriangleAdded(pStoredTris[i]);
	}
#endif //#if 0...
}


#endif // __BANK

void CProcObjectManWrapper::Init(unsigned /*initMode*/)
{
    g_procObjMan.Init();
}

void CProcObjectManWrapper::Shutdown(unsigned /*shutdownMode*/)
{
    g_procObjMan.Shutdown();
}

void CProcObjectManWrapper::Update()
{
    g_procObjMan.Update();
}
