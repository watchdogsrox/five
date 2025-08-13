// File header
#include "ai/EntityScanner.h"

// Rage headers
#include "fwscene/world/WorldRepMulti.h"

// Game headers
#include "Peds/PedIntelligence.h"
#include "Peds/Ped.h"
#include "Physics/Physics.h"
#include "Scene/World/GameWorld.h"

AI_OPTIMISATIONS()

// Profiling
EXT_PF_TIMER(EntityScanner);

// Defines
#define SORT_AFTER		1
#define USE_POOLS		1

static dev_float OBJECT_SCAN_RADIUS = 10.0f;
atQueue<CObjectScanner*,OBJECT_SCANNER_QUEUE_SIZE> CObjectScanner::ms_ObjectScannerUpdateQueue;
CObjectScanner* CObjectScanner::ms_ObjectScannerCurrentlySearching[OBJECT_SCANNER_NUM_CONCURRENT_SEARCHES] = {	NULL, NULL, NULL, NULL,
																												NULL, NULL, NULL, NULL,
																												NULL, NULL, NULL, NULL,
																												NULL, NULL, NULL, NULL	};

fwSearch CObjectScanner::ms_ObjectScannerSearch[OBJECT_SCANNER_NUM_CONCURRENT_SEARCHES] = {	fwSearch(512), fwSearch(512), fwSearch(512), fwSearch(512),
																							fwSearch(512), fwSearch(512), fwSearch(512), fwSearch(512),
																							fwSearch(512), fwSearch(512), fwSearch(512), fwSearch(512),
																							fwSearch(512), fwSearch(512), fwSearch(512), fwSearch(512)	};


////////////////////////////////////////////////////////////////////////////////

CEntityScanner::CEntityScanner(ProcessType processType, s32 iScanPeriod)
: CExpensiveProcess(processType)
{
	m_Timer.SetPeriod(iScanPeriod);
}

////////////////////////////////////////////////////////////////////////////////

CEntityScanner::~CEntityScanner()
{
	Clear();
}

////////////////////////////////////////////////////////////////////////////////

void CEntityScanner::ScanForEntitiesInRange(CEntity& scanningEntity, bool bForceUpdate)
{
	PF_PUSH_TIMEBAR_DETAIL("ScanForEntitiesInRange");

	// Signal started
	StartProcess();

	if(!bForceUpdate)
	{
		if(IsRegistered())
		{
			if(!ShouldBeProcessedThisFrame())
			{
				StopProcess();
				PF_POP_TIMEBAR_DETAIL();
				return;
			}
		}
		else if(!m_Timer.Tick())
		{
			StopProcess();
			PF_POP_TIMEBAR_DETAIL();
			return;
		}
	}

	// Should we scan?
	if(ShouldPerformScan(scanningEntity))
	{
		// Derived scan function
		EntityInfos entities;
		ScanForEntitiesInRangeImpl(scanningEntity, entities);

		// Sort the list
		SortAndStoreEntities(entities);
	}

	// Signal stopped
	StopProcess();

	PF_POP_TIMEBAR_DETAIL();
}

////////////////////////////////////////////////////////////////////////////////

void CEntityScanner::Clear()
{
	int count = m_Entities.GetCount();
	for(s32 i = 0; i < count; i++)
	{
		m_Entities[i] = NULL;
	}
	m_Entities.Reset();
}

////////////////////////////////////////////////////////////////////////////////

#if DEBUG_DRAW

void CEntityScanner::DebugRender(const Color32& col) const
{
	Color32 debugCol(col);
	if(IsRegistered() && ShouldBeProcessedThisFrameSimple())
	{
		debugCol = Color_white;
	}

	for(s32 i = 0; i < m_Entities.GetCount(); i++)
	{
		float fScale = (1.0f - ((float)i / (float)m_Entities.GetCount()));
		debugCol = Color32(debugCol.GetRedf() * fScale, debugCol.GetGreenf() * fScale, debugCol.GetBluef() * fScale);

		if(m_Entities[i])
		{
			spdAABB tempBox;
			const spdAABB &box = m_Entities[i]->GetBoundBox(tempBox);
			Vec3V vExtent = box.GetExtent();
			grcDebugDraw::BoxOriented(-vExtent, vExtent, m_Entities[i]->GetMatrix(), debugCol, false);
		}
	}
}

#endif // DEBUG_DRAW

////////////////////////////////////////////////////////////////////////////////

bool CEntityScanner::ScanForEntitiesInRange_PerEntityCallback(CEntity* pEntity, void* pData)
{
	sEntityScannerCallbackData* pEntityScannerCallbackData = reinterpret_cast<sEntityScannerCallbackData*>(pData);
	EntityInfos& entities = pEntityScannerCallbackData->entities;

	// We don't want to include the scanning pEntity in the list of nearby peds
	if(pEntity == &pEntityScannerCallbackData->scanningEntity)
	{
		return true;
	}
	
	const Vec3V entityPos = pEntity->GetTransform().GetPosition();
	const ScalarV dist2 = DistSquared(entityPos, pEntityScannerCallbackData->vCentre);

	const ScalarV zero(V_ZERO);

	const ScalarV searchRadius2 = pEntityScannerCallbackData->vRadiusSqr;

	if (IsGreaterThanAll(searchRadius2, zero) && IsGreaterThanAll(dist2, searchRadius2))
	{
		return true;
	}

	float fDistSqr = dist2.Getf();
	Assertf(((FloatToIntBitwise(fDistSqr) & 0x80000000) == 0) && ((FloatToIntBitwise(fDistSqr) & 0x7F800000) != 0x7F800000), "Bad float for entity distance!");

#if SORT_AFTER
	if(entities.IsFull())
	{
		s32 iFurthestIndex   = -1;
		f32 fFurthestDistSqr = 0.0f;
		for(s32 i = 0; i < entities.GetCount(); i++)
		{
			float entDistSqr = entities[i].fDistSqr;
			if(entDistSqr > fFurthestDistSqr && entDistSqr > fDistSqr)
			{
				iFurthestIndex   = i;
				fFurthestDistSqr = entDistSqr;
			}
		}

		if(iFurthestIndex == -1)
		{
			// New entity is further away than any in our list
			return true;
		}

		// Delete furthest entity
		entities.DeleteFast(iFurthestIndex);
	}

	// Exclude procedural objects that are tiny
	const float fMinBoundRadiusToBeProcessed = 0.2f;
	const float fEntityBoundRadius = pEntity->GetBoundRadius();
	if(pEntity->GetOwnedBy() != ENTITY_OWNEDBY_PROCEDURAL || fEntityBoundRadius > fMinBoundRadiusToBeProcessed)
	{
		entities.Push(sEntityInfo(pEntity, fDistSqr));
	}
#else // SORT_AFTER
	s32 i;
	for(i = 0; i < entities.GetCount(); i++)
	{
		if(fDistSqr < entities[i].fDistSqr)
		{
			if(entities.IsFull())
			{
				// Array is full, remove the furthest away entity
				entities.Pop();
			}

			sEntityInfo& entry = entities.Insert(i);
			entry.pEntity = pEntity;
			entry.fDistSqr = fDistSqr;
			break;
		}
	}

	if(i == entities.GetCount() && !entities.IsFull())
	{
		// Our new entity must be further away than all the others, so add it to the back
		entities.Push(sEntityInfo(pEntity, fDistSqr));
	}
#endif //SORT_AFTER

	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CEntityScanner::ShouldPerformScan(CEntity& scanningEntity) const
{
	if(scanningEntity.GetIsTypePed())
	{
		CPed& ped = static_cast<CPed&>(scanningEntity);
		if(ped.IsDead())
		{
			// Don't scan if dead
			return false;
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

float CEntityScanner::ComputeRadius(CEntity& scanningEntity) const
{
	if(scanningEntity.GetIsTypePed())
	{
		CPed& scanningPed = static_cast<CPed&>(scanningEntity);
		return rage::Max(scanningPed.GetPedIntelligence()->GetPedPerception().GetSeeingRange(), CPedPerception::ms_fSenseRange);
	}
	else if(scanningEntity.GetIsTypeVehicle())
	{
		static dev_float VEHICLE_SCAN_RADIUS = 50.0f;
		static dev_float FAST_VEHICLE_SCAN_RADIUS = 90.0f;
		static dev_float FAST_VEHICLE_SPEED_THRESHOLD_SQR = 32.0f * 32.0f;

		const float fCurrentVelSqr = static_cast<CVehicle&>(scanningEntity).GetVelocity().Mag2();
		return fCurrentVelSqr > FAST_VEHICLE_SPEED_THRESHOLD_SQR ? FAST_VEHICLE_SCAN_RADIUS : VEHICLE_SCAN_RADIUS;
	}
	else
	{
		return 0.0f;
	}
}

////////////////////////////////////////////////////////////////////////////////

void CEntityScanner::SortAndStoreEntities(EntityInfos& entities)
{
	if(entities.GetCount() > 0)
	{
		int count = entities.GetCount();
#if SORT_AFTER
		std::sort(&entities[0], &entities[0] + count, sEntityInfo::LessThan);
#endif // SORT_AFTER

		// Remove all references that are no longer active.
		int oldCount = m_Entities.GetCount();
		for (int x=count; x<oldCount; x++)
		{
			m_Entities[x] = NULL;
		}

		m_Entities.Resize(count);

		for(s32 i = 0; i < count; i++)
		{
			m_Entities[i] = entities[i].pEntity;
		}
	}
	else
	{
		Clear();
	}
}


bool CompareSpatialArrayResults(const CSpatialArray::FindResult& a, const CSpatialArray::FindResult& b)
{
	return PositiveFloatLessThan_Unsafe(a.m_DistanceSq, b.m_DistanceSq);
}

////////////////////////////////////////////////////////////////////////////////

CPedScanner::CPedScanner(ProcessType processType)
: CEntityScanner(processType)
{
	RegisterSlot();
}

////////////////////////////////////////////////////////////////////////////////

CPedScanner::~CPedScanner()
{
}

////////////////////////////////////////////////////////////////////////////////

void CPedScanner::ScanForEntitiesInRangeImpl(CEntity& scanningEntity, EntityInfos& entities)
{
	// For some reason ignore dead peds if the scanning scanningPed is the player? player targeting perhaps??
	bool bIgnoreDeadPeds = scanningEntity.GetIsTypePed() && FindPlayerPed() == &scanningEntity;

	// Compute the radius for the scan
	float fRadius = ComputeRadius(scanningEntity);

	// Sphere to search for entities inside
	const Vec3V vScanningEntityPosition = scanningEntity.GetTransform().GetPosition();
	const spdSphere scanningSphere(vScanningEntityPosition, ScalarV(fRadius));

#if USE_POOLS
	// TODO: If we add a flag to the spatial array that indicates which peds are dead, we should
	// be able to use that code path for the bIgnoreDeadPeds (player) case too.
	bool useSpatialArray = !bIgnoreDeadPeds;
	if (useSpatialArray && !CPed::ms_spatialArray->IsEmpty())
	{
		if (fRadius <= 0.0f)
		{
			fRadius = FLT_MAX;
		}
		int maxNumPeds = CPed::GetPool()->GetSize();
		CSpatialArray::FindResult* results = Alloca(CSpatialArray::FindResult, maxNumPeds);
		int numFound = CPed::ms_spatialArray->FindInSphere(vScanningEntityPosition, fRadius, results, maxNumPeds);

		if (numFound > MAX_NUM_ENTITIES)
		{
			std::sort(results, results+numFound, CompareSpatialArrayResults);

			// Find our player's spatial array node and if the player is found outside of the closest MAX_NUM_ENTITIES 
			// then replace the last (furthest) entity in the list with the player node
			CPed *pPlayer = CGameWorld::FindLocalPlayer();
			if(pPlayer && pPlayer != &scanningEntity)
			{
				const CSpatialArrayNode& playerNode = pPlayer->GetSpatialArrayNode();
				for(int i = MAX_NUM_ENTITIES; i < numFound; i++)
				{
					if(results[i].m_Node == &playerNode)
					{
						results[MAX_NUM_ENTITIES - 1] = results[i];
						break;
					}
				}
			}

			numFound = MAX_NUM_ENTITIES;
		}

		entities.Reset();
		for(int i = 0; i < numFound; i++)
		{
			CPed* veh = CPed::GetPedFromSpatialArrayNode(results[i].m_Node);
			if (veh != &scanningEntity)
			{
				sEntityInfo& info = entities.Append();
				// We don't want to include the scanning pEntity in the list of nearby vehicles
				info.pEntity = veh;
				info.fDistSqr = results[i].m_DistanceSq;
			}
		}
		return;
	}

	// Create the callback data to be passed through to the callback function
	sPedScannerCallbackData callbackData(scanningEntity, entities, VEC3V_TO_VECTOR3(vScanningEntityPosition), rage::square(fRadius), bIgnoreDeadPeds);

	CPed::Pool* pool = CPed::GetPool();
	s32 i = pool->GetSize();	
	while(i--)
	{
		CPed* pPed = pool->GetSlot(i);
		if(pPed)
		{
			ScanForEntitiesInRange_PerEntityCallback(pPed, (void*)&callbackData);
		}
	}
#else // USE_POOLS
	phIterator iterator(phIterator::PHITERATORLOCKTYPE_READLOCK);
	iterator.InitCull_Sphere(scanningEntity.GetPosition(), fRadius);
	iterator.SetIncludeFlags(ArchetypeFlags::GTA_PED_TYPE);
	iterator.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
	iterator.SetStateIncludeFlags(phLevelBase::STATE_FLAGS_ALL);
	u16 culledLevelIndex = CPhysics::GetLevel()->GetFirstCulledObject(iterator);
	while(culledLevelIndex != phInst::INVALID_INDEX)
	{
		phInst* pCulledInstance = CPhysics::GetLevel()->GetInstance(culledLevelIndex);
		aiAssert(pCulledInstance);

		CEntity* pCulledEntity = CPhysics::GetEntityFromInst(pCulledInstance);
		if( pCulledEntity )
		{
			ScanForEntitiesInRange_PerEntityCallback(pCulledEntity, (void*)&callbackData);
		}

		culledLevelIndex = CPhysics::GetLevel()->GetNextCulledObject(iterator);
	}
#endif // USE_POOLS
}

////////////////////////////////////////////////////////////////////////////////

bool CPedScanner::ScanForEntitiesInRange_PerEntityCallback(CEntity* pEntity, void* pData)
{
	sPedScannerCallbackData* pPedScannerCallbackData = reinterpret_cast<sPedScannerCallbackData*>(pData);

	// Ignore dead peds if instructed too..
	if(pPedScannerCallbackData->bIgnoreDeadPeds)
	{
		if(pEntity->GetIsTypePed() && static_cast<CPed*>(pEntity)->IsDead())
		{
			return true;
		}
	}

	// Base class
	return CEntityScanner::ScanForEntitiesInRange_PerEntityCallback(pEntity, pData);
}

////////////////////////////////////////////////////////////////////////////////

CVehicleScanner::CVehicleScanner(ProcessType processType)
: CEntityScanner(processType)
, m_fTimeLastUpdated(-1.0f)
{
	RegisterSlot();
}

////////////////////////////////////////////////////////////////////////////////

CVehicleScanner::~CVehicleScanner()
{
}

////////////////////////////////////////////////////////////////////////////////

bool CVehicleScanner::ShouldPerformScan(CEntity& scanningEntity) const
{
	// Base Class
	if(!CEntityScanner::ShouldPerformScan(scanningEntity))
	{
		return false;
	}

	if(scanningEntity.GetIsTypePed())
	{
		CPed& ped = static_cast<CPed&>(scanningEntity);
		if(ped.GetVehiclePedInside() != NULL)
		{
			if(!ped.PopTypeIsMission())
			{
				// Don't scan when in a vehicle
				return false;
			}
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////


void CVehicleScanner::ScanForEntitiesInRangeImpl(CEntity& scanningEntity, EntityInfos& entities)
{
	// Compute the radius for the scan
	float fRadius = ComputeRadius(scanningEntity);

	Vec3V vScanningEntityPosition = scanningEntity.GetTransform().GetPosition();

	//Offset scan for velocity
	float fCurrentTime = ((float)fwTimer::GetGameTimer().GetTimeInMilliseconds() ) * 0.001f;
	if (m_fTimeLastUpdated >= 0.0f)
	{
		const CVehicle* scanningVehicle = 0;
		if(scanningEntity.GetIsTypeVehicle())
		{
			scanningVehicle = static_cast<const CVehicle*>(&scanningEntity);
		}
		else if (scanningEntity.GetIsTypePed())
		{
			const CPed& ped = static_cast<CPed&>(scanningEntity);
			scanningVehicle = ped.GetVehiclePedInside();
		}

		if(scanningVehicle)
		{
			ScalarV vLookAheadTime(Min(fCurrentTime - m_fTimeLastUpdated, 0.5f));
			Vec3V vVelocity = RCC_VEC3V(scanningVehicle->GetVelocity());
			vScanningEntityPosition += vVelocity * vLookAheadTime;
		}
	}
	m_fTimeLastUpdated = fCurrentTime;


#if USE_POOLS

	// Do a faster search using the spatial array, if there is data in it
	if (!CVehicle::ms_spatialArray->IsEmpty())
	{
		if (fRadius <= 0.0f)
		{
			fRadius = FLT_MAX;
		}
		int maxNumVehicles = (int) CVehicle::GetPool()->GetSize();
		CSpatialArray::FindResult* results = Alloca(CSpatialArray::FindResult, maxNumVehicles);
		int numFound = CVehicle::ms_spatialArray->FindInSphere(vScanningEntityPosition, fRadius, results, maxNumVehicles);

		if (numFound > MAX_NUM_ENTITIES)
		{
			std::sort(results, results+numFound, CompareSpatialArrayResults);
			numFound = MAX_NUM_ENTITIES;
		}

		entities.Reset();
		for(int i = 0; i < numFound; i++)
		{
			CVehicle* veh = CVehicle::GetVehicleFromSpatialArrayNode(results[i].m_Node);
			if (veh != &scanningEntity)
			{
				sEntityInfo& info = entities.Append();
				// We don't want to include the scanning pEntity in the list of nearby vehicles
				info.pEntity = veh;
				info.fDistSqr = results[i].m_DistanceSq;
				
#if __ASSERT
				// sanity check the position values
				Vec3V arrayPosV;
				CVehicle::GetSpatialArray().GetPosition(*results[i].m_Node, arrayPosV);
				Vec3V vehPos = veh->GetTransform().GetPosition();
				Assertf(IsBetweenNegAndPosBounds(arrayPosV - vehPos, Vec3V(V_FLT_SMALL_6)), "Vehicle spatial array is out of date!");
#endif
			}
		}

#if __ASSERT
		if(entities.GetCount() == 0)
		{
			// This stuff is basically a more informative assert so we can track down cases
			// where we scanned but didn't find the vehicle we are supposedly inside.
			if(scanningEntity.GetIsTypePed())
			{
				const CPed& ped = static_cast<CPed&>(scanningEntity);
				const CVehicle* pInside = ped.GetVehiclePedInside();
				if(pInside)
				{
					const Vec3V pedPosV = ped.GetTransform().GetPosition();
					const Vec3V vehPosV = pInside->GetTransform().GetPosition();
					const CSpatialArrayNode& node = pInside->GetSpatialArrayNode();
					if(node.IsInserted())
					{
						//Vec3V arrayPosV;
						//CVehicle::GetSpatialArray().GetPosition(node, arrayPosV);

						//Assertf(0,
						//		"Scanning ped is inside a vehicle that's in the spatial array, but not in the right position. "
						//		"Ped is at %.1f, %.1f, %.1f, vehicle is at %.1f, %.1f, %.1f, but its spatial array position is %.1f, %.1f, %.1f. "
						//		"Ped model is %s, vehicle model is %s.",
						//		pedPosV.GetXf(), pedPosV.GetYf(), pedPosV.GetZf(),
						//		vehPosV.GetXf(), vehPosV.GetYf(), vehPosV.GetZf(),
						//		arrayPosV.GetXf(), arrayPosV.GetYf(), arrayPosV.GetZf(),
						//		ped.GetModelName(), pInside->GetModelName()
						//		);
					}
					else
					{
						Assertf(0,
								"Scanning ped is inside a vehicle that's not in the spatial array. "
								"Ped is at %.1f, %.1f, %.1f, vehicle is at %.1f, %.1f, %.1f. "
								"Ped model is %s, vehicle model is %s.",
								pedPosV.GetXf(), pedPosV.GetYf(), pedPosV.GetZf(),
								vehPosV.GetXf(), vehPosV.GetYf(), vehPosV.GetZf(),
								ped.GetModelName(), pInside->GetModelName()
								);

					}
				}
			}
		}
#endif	// __ASSERT

		return;
	}

	// Sphere to search for entities inside
	const spdSphere scanningSphere(vScanningEntityPosition, ScalarV(fRadius));

	ScalarV vSearchRadius2(square(fRadius));

	if (fRadius <= 0.0f)
	{
		vSearchRadius2 = ScalarV(V_INF);
	}

	CVehicle::Pool* pool = CVehicle::GetPool();
	s32 i = (s32) pool->GetSize();	
	while(i--)
	{
		CVehicle* pVehicle = pool->GetSlot(i);
		if(pVehicle)
		{	
			// We don't want to include the scanning pEntity in the list of nearby vehicles
			if(pVehicle == &scanningEntity)
			{
				continue;
			}

			Vec3V entityPos = pVehicle->GetTransform().GetPosition();
			ScalarV dist2 = DistSquared(entityPos, vScanningEntityPosition);

			if (IsGreaterThanAll(dist2, vSearchRadius2))
			{
				continue;
			}

			float fDistSqr = dist2.Getf();

			if(entities.IsFull())
			{
				s32 iFurthestIndex   = -1;
				f32 fFurthestDistSqr = 0.0f;   
				int entCount = entities.GetCount();
				for(s32 i = 0; i < entCount; i++)
				{			 
					float entDistSqr = entities[i].fDistSqr;
					if(entDistSqr > fFurthestDistSqr && entDistSqr > fDistSqr)
					{
						iFurthestIndex   = i;
						fFurthestDistSqr = entDistSqr;
					}
				}

				if(iFurthestIndex == -1)
				{
					// New entity is further away than any in our list
					continue;
				}

				// Delete furthest entity
				entities.DeleteFast(iFurthestIndex);
			}

			// Add
			entities.Push(sEntityInfo(pVehicle, fDistSqr));
		}
	}

#else // USE_POOLS
	phIterator iterator(phIterator::PHITERATORLOCKTYPE_READLOCK);
	iterator.InitCull_Sphere(scanningEntity.GetPosition(), fRadius);
	iterator.SetIncludeFlags(ArchetypeFlags::GTA_VEHICLE_TYPE);
	iterator.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
	iterator.SetStateIncludeFlags(phLevelBase::STATE_FLAGS_ALL);
	u16 culledLevelIndex = CPhysics::GetLevel()->GetFirstCulledObject(iterator);
	while(culledLevelIndex != phInst::INVALID_INDEX)
	{
		phInst* pCulledInstance = CPhysics::GetLevel()->GetInstance(culledLevelIndex);
		aiAssert(pCulledInstance);

		CEntity* pCulledEntity = CPhysics::GetEntityFromInst(pCulledInstance);
		if( pCulledEntity )
		{
			ScanForEntitiesInRange_PerEntityCallback(pCulledEntity, (void*)&callbackData);
		}
		
		culledLevelIndex = CPhysics::GetLevel()->GetNextCulledObject(iterator);
	}
#endif // USE_POOLS

	// If we have no entities nearby
	if(entities.GetCount() == 0)
	{
		if(scanningEntity.GetIsTypePed())
		{
			CPed& scanningPed = static_cast<CPed&>(scanningEntity);

			CVehicle* pVehicle = scanningPed.GetVehiclePedInside();
			if(pVehicle)
			{
				// Add our own vehicle
				entities.Push(sEntityInfo(pVehicle, 0.0f));
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
dev_float CDoorScanner::ms_fScanningRadius = 10.0f;

CDoorScanner::CDoorScanner(ProcessType processType)
: CEntityScanner(processType )
{
	// Register scanner with expensive process
	RegisterSlot();
}

////////////////////////////////////////////////////////////////////////////////

CDoorScanner::~CDoorScanner()
{
}

////////////////////////////////////////////////////////////////////////////////

void CDoorScanner::ScanForEntitiesInRangeImpl(CEntity& scanningEntity, EntityInfos& entities)
{
	// Sphere to search for entities inside
	const Vector3 vScanningEntityPosition = VEC3V_TO_VECTOR3( scanningEntity.GetTransform().GetPosition() );
	
	// Create the callback data to be passed through to the callback function
	sDoorScannerCallbackData callbackData( scanningEntity, entities, vScanningEntityPosition, rage::square( ms_fScanningRadius ) );

#if ENABLE_PHYSICS_LOCK
	phIterator iterator( phIterator::PHITERATORLOCKTYPE_READLOCK );
#else	// ENABLE_PHYSICS_LOCK
	phIterator iterator;
#endif	// ENABLE_PHYSICS_LOCK
	iterator.InitCull_Sphere( vScanningEntityPosition, ms_fScanningRadius );
	iterator.SetIncludeFlags( ArchetypeFlags::GTA_OBJECT_TYPE );
	iterator.SetTypeFlags( ArchetypeFlags::GTA_AI_TEST );
	iterator.SetStateIncludeFlags( phLevelBase::STATE_FLAGS_ALL );
	u16 culledLevelIndex = CPhysics::GetLevel()->GetFirstCulledObject(iterator);
	while(culledLevelIndex != phInst::INVALID_INDEX)
	{
		phInst* pCulledInstance = CPhysics::GetLevel()->GetInstance(culledLevelIndex);
		aiAssert(pCulledInstance);

		CEntity* pCulledEntity = CPhysics::GetEntityFromInst(pCulledInstance);
		if( pCulledEntity )
		{
			ScanForEntitiesInRange_PerEntityCallback(pCulledEntity, (void*)&callbackData);
		}
			
		culledLevelIndex = CPhysics::GetLevel()->GetNextCulledObject(iterator);
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CDoorScanner::ScanForEntitiesInRange_PerEntityCallback(CEntity* pEntity, void* pData)
{
	aiAssert(pEntity);

	if( !((CObject*)pEntity)->IsADoor() )
	{
		return true;
	}

	// Base class
	return CEntityScanner::ScanForEntitiesInRange_PerEntityCallback(pEntity, pData);
}

////////////////////////////////////////////////////////////////////////////////

CObjectScanner::CObjectScanner(ProcessType processType)
:  CEntityScanner(processType, 48)
, m_fTimeLastUpdated(-1.0f)
{
	if (processType == CExpensiveProcess::VPT_ObjectScanner)
	{
		RegisterSlot();
	}
}

////////////////////////////////////////////////////////////////////////////////

CObjectScanner::~CObjectScanner()
{
	this->RemoveObjectScannerFromUpdateQueue();
}

void CObjectScanner::ProcessScanningEntityPosition(CEntity& scanningEntity, Vec3V_Ref vScanPositionInOut)
{
	//Offset scan for velocity
	float fCurrentTime = ((float)fwTimer::GetGameTimer().GetTimeInMilliseconds() ) * 0.001f;
	if (m_fTimeLastUpdated >= 0.0f)
	{
		const CVehicle* scanningVehicle = 0;
		if(scanningEntity.GetIsTypeVehicle())
		{
			scanningVehicle = static_cast<const CVehicle*>(&scanningEntity);
		}
		else if (scanningEntity.GetIsTypePed())
		{
			const CPed& ped = static_cast<CPed&>(scanningEntity);
			scanningVehicle = ped.GetVehiclePedInside();
		}

		if(scanningVehicle)
		{
			ScalarV vLookAheadTime(Min(fCurrentTime - m_fTimeLastUpdated, 0.5f));
			Vec3V vVelocity = RCC_VEC3V(scanningVehicle->GetVelocity());
			vScanPositionInOut += vVelocity * vLookAheadTime;
		}
	}
	m_fTimeLastUpdated = fCurrentTime;
}

////////////////////////////////////////////////////////////////////////////////

void CObjectScanner::ScanForEntitiesInRangeImpl(CEntity& scanningEntity, EntityInfos& entities)
{
	PF_AUTO_PUSH_TIMEBAR("CObjectScanner::ScanForEntitiesInRangeImpl");

	Vec3V vScanningEntityPosition = scanningEntity.GetTransform().GetPosition();
	ProcessScanningEntityPosition(scanningEntity, vScanningEntityPosition);

	// Sphere to search for entities inside
	const spdSphere scanningSphere(vScanningEntityPosition, ScalarV(OBJECT_SCAN_RADIUS));

// 	if (scanningEntity.GetIsTypeVehicle())
// 	{
// 		grcDebugDraw::Sphere(vScanningEntityPosition, OBJECT_SCAN_RADIUS, Color_salmon, false, -1);
// 	}

	// Create the callback data to be passed through to the callback function
	sEntityScannerCallbackData callbackData(scanningEntity, entities, VEC3V_TO_VECTOR3(vScanningEntityPosition), 0.0f);

	// Scan through all intersecting entities finding all entities before
	fwIsSphereIntersecting intersection(scanningSphere);
	CGameWorld::ForAllEntitiesIntersecting(&intersection, CObjectScanner::ScanForEntitiesInRange_PerEntityCallback, (void*)&callbackData, ENTITY_TYPE_MASK_OBJECT, SEARCH_LOCATION_INTERIORS|SEARCH_LOCATION_EXTERIORS, SEARCH_LODTYPE_HIGHDETAIL, SEARCH_OPTION_FORCE_PPU_CODEPATH|SEARCH_OPTION_SKIP_PROCEDURAL_ENTITIES, WORLDREP_SEARCHMODULE_PEDS);
}

void CObjectScanner::Clear()
{
	// If someone is clearing the object list, we then assume then don't want 
	// the async update to finish sometime later and populate it.
	this->RemoveObjectScannerFromUpdateQueue();

	CEntityScanner::Clear();
}

void CObjectScanner::RemoveObjectScannerFromUpdateQueue()
{
#if __ASSERT
	for(u32 i = 0; i < OBJECT_SCANNER_NUM_CONCURRENT_SEARCHES; ++i)
	{
		Assert(ms_ObjectScannerCurrentlySearching[i] != this);
	}
#endif

	int index = -1;
	if(ms_ObjectScannerUpdateQueue.Find(this, &index))
	{
		ms_ObjectScannerUpdateQueue.Delete(index);
	}
}

bool CObjectScanner::StartAsyncUpdate(fwSearch* pSearchToUse)
{
	Assert(pSearchToUse);
	if(Unlikely(!m_LastScanningEntity))
	{
		return false;
	}

	PF_PUSH_TIMEBAR("CObjectScanner::StartAsyncUpdate");

	// create search volume
	Vec3V vScanningEntityPosition = m_LastScanningEntity->GetTransform().GetPosition();
	ProcessScanningEntityPosition(*m_LastScanningEntity, vScanningEntityPosition);

// 	if (m_LastScanningEntity->GetIsTypeVehicle())
// 	{
// 		grcDebugDraw::Sphere(vScanningEntityPosition, OBJECT_SCAN_RADIUS, Color_salmon, false, -1);
// 	}

	// Sphere to search for entities inside
	const spdSphere searchSphere(vScanningEntityPosition, ScalarV(OBJECT_SCAN_RADIUS));
	fwIsSphereIntersecting searchVol(searchSphere);

	// start spu search
	pSearchToUse->Start(
		fwWorldRepMulti::GetSceneGraphRoot(),								// root scene graph node
		&searchVol,															// search volume
		ENTITY_TYPE_MASK_OBJECT,											// entity types
		SEARCH_LOCATION_EXTERIORS | SEARCH_LOCATION_INTERIORS,				// location flags
		SEARCH_LODTYPE_HIGHDETAIL,											// lod flags
		SEARCH_OPTION_NONE,													// option flags
		WORLDREP_SEARCHMODULE_PEDS,											// module
		sysDependency::kPriorityCritical									// priority
		);

	PF_POP_TIMEBAR();

	return true;
}
void CObjectScanner::EndAsyncUpdate(fwSearch* pSearchToUse)
{
	PF_PUSH_TIMEBAR("CObjectScanner::EndAsyncUpdate");

	Assert(pSearchToUse);
	pSearchToUse->Finalize();

	Assert(m_LastScanningEntity);
	// Create the callback data to be passed through to the callback function
	EntityInfos entities;
	sEntityScannerCallbackData callbackData(*m_LastScanningEntity, entities, VEC3V_TO_VECTOR3(m_LastScanningEntity->GetTransform().GetPosition()), 0.0f);
	pSearchToUse->ExecuteCallbackOnResult(CObjectScanner::AsyncUpdateResultsCB, (void*) &callbackData);

	// Sort the list
	SortAndStoreEntities(entities);

	PF_POP_TIMEBAR();
}

bool CObjectScanner::AsyncUpdateResultsCB(fwEntity* pEntity, void* pData)
{
	return CObjectScanner::ScanForEntitiesInRange_PerEntityCallback((CEntity*) pEntity, pData);
}

void CObjectScanner::ScanImmediately()
{
	if(m_LastScanningEntity)
	{
		// Derived scan function
		EntityInfos entities;
		ScanForEntitiesInRangeImpl(*m_LastScanningEntity, entities);

		// Sort the list
		SortAndStoreEntities(entities);
	}
}

#if __STATS
PF_PAGE(ObjectScannerPage, "Object Scanner");
PF_GROUP(ObjectScannerGroup);
PF_LINK(ObjectScannerPage, ObjectScannerGroup);
PF_TIMER(ScanImmediately_Forced, ObjectScannerGroup);
PF_TIMER(ScanImmediately_QueueFull, ObjectScannerGroup);
PF_COUNTER(UpdateQueue_Queue, ObjectScannerGroup);
PF_VALUE_INT(UpdateQueue_Size, ObjectScannerGroup);
#endif // __STATS

void CObjectScanner::ScanForEntitiesInRange(CEntity& scanningEntity, bool bForceUpdate)
{
	// Signal started
	StartProcess();

	if(!bForceUpdate)
	{
		if(IsRegistered())
		{
			if(!ShouldBeProcessedThisFrame())
			{
				StopProcess();
				return;
			}
		}
		else if(!m_Timer.Tick())
		{
			StopProcess();
			return;
		}
	}

	// Should we scan?
	if(ShouldPerformScan(scanningEntity))
	{
		m_LastScanningEntity = &scanningEntity;
		if(bForceUpdate)
		{
			PF_START(ScanImmediately_Forced);
			this->ScanImmediately();
			PF_STOP(ScanImmediately_Forced);

			// Since we just did an immediate scan, find out if 
			// it is already in the queue. If so, remove it.
			int index = -1;
			if(ms_ObjectScannerUpdateQueue.Find(this, &index))
			{
				ms_ObjectScannerUpdateQueue.Delete(index);
			}
		}
		else if(!ms_ObjectScannerUpdateQueue.Find(this))
		{
			PF_INCREMENT(UpdateQueue_Queue);
			PF_SET(UpdateQueue_Size, ms_ObjectScannerUpdateQueue.GetSize());
			// If it is not already in the queue, then we need to update it.
			// If the queue is full, do an immediate scan/update. This should 
			// be rare (when you respawn and lots of peds/vehicles get spawned 
			// within a few frames, for instance). Otherwise, we push the 
			// object scanner onto the queue to get updated asynchronously.
			if(ms_ObjectScannerUpdateQueue.IsFull())
			{
				// If we're unable to keep up (requests carrying over from the previous frame),
				// consider increasing OBJECT_SCANNER_NUM_CONCURRENT_SEARCHES.
				//
				// If we're filling the entire queue in a single frame, investigate intention,
				// and, if necessary, increase OBJECT_SCANNER_QUEUE_SIZE.
				Warningf("Object scanner update queue full");
				PF_START(ScanImmediately_QueueFull);
				this->ScanImmediately();
				PF_STOP(ScanImmediately_QueueFull);
			}
			else
			{
				ms_ObjectScannerUpdateQueue.Push(this);
			}
		}
	}

	// Signal stopped
	StopProcess();
}

void CObjectScanner::StartAsyncUpdateOfObjectScanners()
{
	for(u32 i = 0; i < OBJECT_SCANNER_NUM_CONCURRENT_SEARCHES && ms_ObjectScannerUpdateQueue.GetCount() > 0; ++i)
	{
		ms_ObjectScannerCurrentlySearching[i] = NULL;

		while(ms_ObjectScannerUpdateQueue.GetCount() > 0)
		{
			CObjectScanner* pObjectScanner = ms_ObjectScannerUpdateQueue.Pop();

			bool bStartedSearch = pObjectScanner->StartAsyncUpdate(&ms_ObjectScannerSearch[i]);

			if(bStartedSearch)
			{
				ms_ObjectScannerCurrentlySearching[i] = pObjectScanner;
				break;
			}
		}
	}
}

void CObjectScanner::EndAsyncUpdateOfObjectScanners()
{
	for(u32 i = 0; i < OBJECT_SCANNER_NUM_CONCURRENT_SEARCHES; ++i)
	{
		CObjectScanner* pObjectScanner = ms_ObjectScannerCurrentlySearching[i];
		ms_ObjectScannerCurrentlySearching[i] = NULL;

		if(pObjectScanner)
		{
			pObjectScanner->EndAsyncUpdate(&ms_ObjectScannerSearch[i]);
		}
	}
}
