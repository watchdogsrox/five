
/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    pedplacement.cpp
// PURPOSE : One or two handy functions to deal with the placement of
//			 peds. It seemed handy to put this in a separate file since
//			 it was needed in so many different bits of the code.
//			 (player leaving the car, ped leaving a car, cops generated
//			 for a roadblock etc)
// AUTHOR :  Obbe.
// CREATED : 15/08/00
//
/////////////////////////////////////////////////////////////////////////////////

#include "ModelInfo\PedModelInfo.h"
#include "ModelInfo\VehicleModelInfo.h"
#include "scene\world\gameWorld.h"
#include "Peds\pedplacement.h"
#include "Peds\pedtype.h"
#include "peds/Ped.h"
#include "Physics\GtaArchetype.h"
#include "Physics\GtaInst.h"
#include "Physics\Physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "vehicles/vehicle.h"

AI_OPTIMISATIONS()

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IsPositionClearForPed
// PURPOSE :  Works out whether we can place a ped at these coordinates.
//			  (Whether these coordinates are clear)
/////////////////////////////////////////////////////////////////////////////////

bool CPedPlacement::IsPositionClearForPed(const Vector3& vPos, float fRange,  bool useBoundBoxes, int MaxNum, CEntity** pKindaCollidingObjects, bool considerVehicles, bool considerPeds, bool considerObjects)
{
	s32 NumObjectsInRange;

    if(-1.0f==fRange)
    {
        fRange=0.75f;
    }
    
    if(-1==MaxNum)
    {
        MaxNum=2;
    }
    
	CGameWorld::FindObjectsKindaColliding( vPos, fRange, useBoundBoxes, true, &NumObjectsInRange, MaxNum, pKindaCollidingObjects, false, considerVehicles, considerPeds, considerObjects, considerObjects);	// Just Cars and Peds we're interested in
	if (NumObjectsInRange == 0)
	{
		return (TRUE);
	}
	else
	{
		return (FALSE);
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IsPositionClearOfCars
// PURPOSE :  Works out whether we can place a ped at these coordinates.
//			  (Whether these coordinates are clear)
/////////////////////////////////////////////////////////////////////////////////

CEntity *CPedPlacement::IsPositionClearOfCars(const Vector3 *pCoors)
{
	WorldProbe::CShapeTestSphereDesc sphereDesc;
	WorldProbe::CShapeTestFixedResults<> sphereResults;
	sphereDesc.SetResultsStructure(&sphereResults);
	sphereDesc.SetSphere((*pCoors), 0.25f);
	sphereDesc.SetIncludeFlags(ArchetypeFlags::GTA_VEHICLE_TYPE);

	if(WorldProbe::GetShapeTestManager()->SubmitTest(sphereDesc))
	{
		phInst* pHitInst = sphereResults[0].GetHitInst();
		Assert(pHitInst);
		Assert(CPhysics::GetEntityFromInst(pHitInst));
		Assert(CPhysics::GetEntityFromInst(pHitInst)->GetType()==ENTITY_TYPE_VEHICLE);
		return CPhysics::GetEntityFromInst(pHitInst);
	}
	else
	{
		return 0;
	}
}

//static CColPoint aTempPedColPts[PHYSICAL_MAXNOOFCOLLISIONPOINTS];

CEntity* CPedPlacement::IsPositionClearOfCars(const CPed* pPed)
{
	WorldProbe::CShapeTestSphereDesc sphereDesc;
	WorldProbe::CShapeTestFixedResults<> sphereResults;
	sphereDesc.SetResultsStructure(&sphereResults);
	sphereDesc.SetSphere(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), 0.25f);
	sphereDesc.SetIncludeFlags(ArchetypeFlags::GTA_VEHICLE_TYPE);

	if(WorldProbe::GetShapeTestManager()->SubmitTest(sphereDesc))
	{
		phInst* p = sphereResults[0].GetHitInst();
		Assert(p);
		Assert(CPhysics::GetEntityFromInst(p));
		Assert(CPhysics::GetEntityFromInst(p)->GetType()==ENTITY_TYPE_VEHICLE);
		CVehicle* pVehicle=(CVehicle*)(CPhysics::GetEntityFromInst(p));

		if(pVehicle->GetVehicleType()!=VEHICLE_TYPE_CAR)
		{
			MUST_FIX_THIS(sandy - need equivalent for rage bounds testing);
//RAGE		if(CCollision::ProcessColModels(pPed->GetMatrix(), CModelInfo::GetColModel(pPed->m_nModelIndex), pVehicle->GetMatrix(), CModelInfo::GetColModel(pVehicle->GetModelIndex()), aTempPedColPts))
//RAGE		{
//RAGE			return CPhysics::GetEntityFromInst(p);
//RAGE		}
//RAGE		else
			{
				return 0;
			}
		}
		else if(pVehicle->m_nVehicleFlags.bIsBig)
		{
			MUST_FIX_THIS(sandy - need equivalent for rage bounds testing);
//RAGE		if(CCollision::ProcessColModels(pPed->GetMatrix(), CModelInfo::GetColModel(pPed->m_nModelIndex), pVehicle->GetMatrix(), CModelInfo::GetColModel(pVehicle->GetModelIndex()), aTempPedColPts))
//RAGE		{
//RAGE			return CPhysics::GetEntityFromInst(p);
//RAGE		}
//RAGE		else
			{
				return 0;
			}
		}
		else
		{
			return CPhysics::GetEntityFromInst(p);
		}
	}
	else
	{
		return 0;
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindZCoorForPed
// PURPOSE :  Finds coordinates to place the ped at. (As close to the given
//			  z coordinate as possible but whilst making sure there is enough
//			  headroom.
/////////////////////////////////////////////////////////////////////////////////
#define NUM_INTERSECTIONS 4
bool CPedPlacement::FindZCoorForPed(const float, Vector3 *pCoors, const CEntity* pException, s32* piRoomId, CInteriorInst** ppInteriorInst, float fZAmountAbove, float fZAmountBelow, const u32 iFlags, bool* pbOnDynamicCollision, bool bIgnoreGroundOffset)
{
	Vector3 vecLineStart, vecLineEnd;

	// We have to find a proper z value so that the guy doesn't end up
	// on a bridge that we may be under and stuff. (or under a bridge
	// that we are sitting on)
	// If there	is a surface in the metre above us we'll take the
	// guy up to it.

	// Make sure we're at least above the lowest surface here
	vecLineStart = (*pCoors);
	vecLineStart.z = pCoors->z + fZAmountAbove;
	vecLineEnd = (*pCoors);
	vecLineEnd.z = pCoors->z - fZAmountBelow;

	float Z1 = vecLineEnd.z, Z2 = vecLineEnd.z, MaxZ;

	if( piRoomId )
		*piRoomId = 0;

	bool bLosFound = false;

	// Use a process LOS so we can get back multiple intersections to find any room id needed
	WorldProbe::CShapeTestProbeDesc probeDesc;
	WorldProbe::CShapeTestFixedResults<NUM_INTERSECTIONS> probeResults;
	probeDesc.SetResultsStructure(&probeResults);
	probeDesc.SetStartAndEnd(vecLineStart, vecLineEnd);
	probeDesc.SetIncludeFlags(iFlags);
	probeDesc.SetExcludeEntity(pException);

	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
	{
		WorldProbe::ResultIterator it;
		for(it = probeResults.begin(); it < probeResults.end(); ++it)
		{
			if( it->GetHitDetected() )
			{
				if(!bLosFound && piRoomId && (*piRoomId == 0) )
				{
					*piRoomId = PGTAMATERIALMGR->UnpackRoomId(it->GetHitMaterialId());
					if( ppInteriorInst )
					{
						if ( (*piRoomId != 0) && (it->GetHitInst()!=NULL) )
						{
							CEntity* pHitEntity  = reinterpret_cast<CEntity*>(it->GetHitInst()->GetUserData());
							CInteriorProxy* pInteriorProxy = CInteriorProxy::GetInteriorProxyFromCollisionData(pHitEntity, &it->GetHitInst()->GetPosition());
							Assert(pInteriorProxy);

							CInteriorInst* pIntInst = NULL;
							if (pInteriorProxy){
								pIntInst = pInteriorProxy->GetInteriorInst();
							}

							if (pIntInst && pIntInst->CanReceiveObjects())
							{
								*ppInteriorInst = pInteriorProxy->GetInteriorInst();
							} 
							else
							{
								//if the interior is not ready yet then we should not return a valid room id.
								*piRoomId = 0;
								return(false);			// quite immediately if interior not ready
							}
						}
					}
				}

				Z1 = MAX(it->GetHitPosition().z, Z1);
				bLosFound = true;

				// If info on whether the ped is on dynamic collision was requested, fill it in.
				if( pbOnDynamicCollision )
				{
					phInst* pHitInst = it->GetHitInst();
					if( pHitInst )
					{
						CEntity *pHitEntity = (CEntity *)CPhysics::GetEntityFromInst(pHitInst);
						if( pHitEntity && pHitEntity->GetIsDynamic() )
							*pbOnDynamicCollision = true;
					}
				}
			}
		}
	}
	vecLineStart.x += 0.1f;	// Again to detect small gaps in map
	vecLineStart.y += 0.1f;
	vecLineEnd = vecLineStart;
	vecLineEnd.z = pCoors->z - fZAmountBelow;

	WorldProbe::CShapeTestProbeDesc probeDesc2;
	probeResults.Reset();
	probeDesc2.SetResultsStructure(&probeResults);
	probeDesc2.SetStartAndEnd(vecLineStart, vecLineEnd);
	probeDesc2.SetIncludeFlags(iFlags);
	probeDesc2.SetExcludeEntity(pException);

	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc2))
	{
		WorldProbe::ResultIterator it;
		for(it = probeResults.begin(); it < probeResults.end(); ++it)
		{
			if( it->GetHitDetected() )
			{
				Z2 = MAX(it->GetHitPosition().z, Z2);
				bLosFound = true;
				// If info on whether the ped is on dynamic collision was requested, fill it in.
				if( pbOnDynamicCollision )
				{
					phInst* pHitInst = it->GetHitInst();
					if( pHitInst )
					{
						CEntity *pHitEntity = (CEntity *)CPhysics::GetEntityFromInst(pHitInst);
						if( pHitEntity && pHitEntity->GetIsDynamic() )
							*pbOnDynamicCollision = true;
					}
				}
			}
		}
	}
	
	MaxZ = MAX(Z1, Z2);
	
	if (bLosFound)
	{
		pCoors->z = MaxZ;

		if(!bIgnoreGroundOffset)
		{
			pCoors->z += PED_HUMAN_GROUNDTOROOTOFFSET; // ground pos plus ped offset
		}
	}	
	else
	{	
		return false;
	}

	return true;
}





