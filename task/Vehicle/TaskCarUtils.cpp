// Framework headers
#include "fwanimation/animmanager.h"
#include "fwmaths/Angle.h"
#include "fwmaths/Vector.h"

// Game headers

#include "animation/AnimDefines.h"
#include "camera/CamInterface.h"
#include "Event/Events.h"
#include "ModelInfo/VehicleModelInfo.h"
#include "PedGroup/PedGroup.h"
#include "Peds/Ped.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedMoveBlend/PedMoveBlendManager.h"
#include "Peds/PedPlacement.h"
#include "Peds/Ped.h"
#include "Physics/GtaArchetype.h"
#include "Physics/GtaInst.h"
#include "Physics/Physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "Renderer/Hierarchyids.h"
#include "Script/Script.h"
#include "script/script_cars_and_peds.h"
#include "Streaming/Streaming.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskCarAccessories.h"
#include "Task/Vehicle/TaskCarUtils.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/System/TaskHelpers.h"
#include "Vehicles/Automobile.h"
#include "Vehicles/Bike.h"
#include "Vehicles/Vehicle.h"
#include "Vehicles/Metadata/VehicleLayoutInfo.h"
#include "Vfx/Misc/Fire.h"

//network includes
#include "Network/NetworkInterface.h"

//rage headers
#include "crSkeleton/Skeleton.h"

AI_OPTIMISATIONS()

bool CCarEnterExit::GetNearestCarDoor(const CPed& ped, CVehicle& vehicle, Vector3& vDoorPos, int& iEntryPointIndex)
{
	const bool bDontCheckCollision = (ped.IsLocalPlayer() && vehicle.PopTypeIsMission()) || 
		(vehicle.InheritsFromBoat());

	SeatRequestType seatRequirement = SR_Prefer;
	s32 iSeat = 0;
	if( vehicle.GetVehicleType() == VEHICLE_TYPE_TRAIN )
	{
		seatRequirement = SR_Any;
		iSeat = -1;
	}
	
	iEntryPointIndex = CModelSeatInfo::EvaluateSeatAccessFromAllEntryPoints( &ped, &vehicle, seatRequirement, iSeat, bDontCheckCollision);
	if( iEntryPointIndex != -1 )
	{
		const CEntryExitPoint* pEntryPoint = vehicle.GetEntryExitPoint(iEntryPointIndex);
		return pEntryPoint->GetEntryPointPosition(&vehicle, &ped, vDoorPos);
	}
	return false;
}

bool CCarEnterExit::GetNearestCarSeat(const CPed& ped, CVehicle& vehicle, Vector3& vSeatPos, int& iSeatIndex)
{
	// Player can warp into the car if its a mission car, or if there is a friendly mission ped that can't be jacked in one of the passenger seats.
	bool bPlayerCanWarpIntoCar = CPlayerInfo::CanPlayerWarpIntoVehicle(ped, vehicle);
	bool bWarpIntoVehicle = CTaskVehicleFSM::CanPedWarpIntoVehicle(ped, vehicle);

	const bool bDontCheckCollision = bPlayerCanWarpIntoCar || (vehicle.InheritsFromBoat()) || (ped.GetIsInWater() && vehicle.HasWaterEntry()) || bWarpIntoVehicle;

	SeatRequestType seatRequirement = SR_Prefer;
	s32 iSeatRequest = 0;

	if( vehicle.GetLayoutInfo() && !vehicle.GetLayoutInfo()->GetHasDriver())
	{
		seatRequirement = SR_Any;
		iSeatRequest = -1;
	}

	bool bTechnical2InWater = MI_CAR_TECHNICAL2.IsValid() && vehicle.GetModelIndex() == MI_CAR_TECHNICAL2 && vehicle.GetIsInWater();

	VehicleEnterExitFlags flags;
	if (NetworkInterface::IsGameInProgress() && (ped.GetPedConfigFlag(CPED_CONFIG_FLAG_PlayerCanJackFriendlyPlayers) || bTechnical2InWater))
	{
		flags.BitSet().Set(CVehicleEnterExitFlags::ConsiderJackingFriendlyPeds);
	}

	s32 iTargetEntryPoint = CModelSeatInfo::EvaluateSeatAccessFromAllEntryPoints( &ped, &vehicle, seatRequirement, iSeatRequest, bDontCheckCollision, flags );
//	s32 iTargetEntryPoint = CTaskVehicleFSM::EvaluateAllEntryPoints( &ped, &vehicle, seatRequirement, iSeatRequest, bDontCheckCollision, 0 );
	if( iTargetEntryPoint != -1 )
	{
		const CEntryExitPoint* pEntryPoint = vehicle.GetEntryExitPoint(iTargetEntryPoint);
		iSeatIndex = pEntryPoint->GetSeat(SA_directAccessSeat);

		if (vehicle.IsTurretSeat(iSeatIndex))
		{
			s32 nFlags = 0;
			nFlags |= CModelSeatInfo::EF_RoughPosition;
			Quaternion qEntryOrientation(0.0f,0.0f,0.0f,1.0f);
			vehicle.GetVehicleModelInfo()->GetModelSeatInfo()->CalculateEntrySituation(&vehicle, &ped, vSeatPos, qEntryOrientation,  iTargetEntryPoint, nFlags);

			return true;
		}
		else
		{
			Matrix34 localSeatMatrix, matResult;
			const int boneId = iSeatIndex > -1 ? vehicle.GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromSeat(iSeatIndex) : -1;
			Assertf(boneId!=-1, "%s:GetEnterCarMatrix - Bone does not exist", vehicle.GetVehicleModelInfo()->GetModelName());
			if(boneId!=-1)
			{
				localSeatMatrix = vehicle.GetLocalMtx(boneId);
				Matrix34 m = MAT34V_TO_MATRIX34(vehicle.GetMatrix());
				matResult.Dot(localSeatMatrix, m);
				vSeatPos = matResult.d;

				return true;
			}
		}

	}
	return false;
}

bool CCarEnterExit::CarHasDoorToOpen(CVehicle& vehicle, int iEntryPointIndex)
{
	int iDoorBoneIndex = vehicle.GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromEntryPoint(iEntryPointIndex);
	CCarDoor* pDoor = vehicle.GetDoorFromBoneIndex(iDoorBoneIndex);
	return (pDoor && pDoor->GetIsIntact(&vehicle) && !pDoor->GetIsFullyOpen());
}

bool CCarEnterExit::CarHasDoorToClose(CVehicle& vehicle, int iDoorBoneIndex)
{
	CCarDoor* pDoor = vehicle.GetDoorFromBoneIndex(iDoorBoneIndex);
	return (pDoor && pDoor->GetIsIntact(&vehicle) && !pDoor->GetIsClosed());
}

bool CCarEnterExit::CarHasOpenableDoor(CVehicle& vehicle, int UNUSED_PARAM(iDoorBoneIndex), const CPed& ped)
{
	return (vehicle.CanPedOpenLocks(&ped));
}


bool CCarEnterExit::IsVehicleStealable(const CVehicle& vehicleToSteal, const CPed& ped, const bool bIgnoreLawEnforcementVehicles, const bool bIgnoreVehicleHealth )
{
	CVehicle& vehicle=(CVehicle&)vehicleToSteal;
	if( vehicle.GetVehicleType()!=VEHICLE_TYPE_PLANE && vehicle.GetVehicleType()!=VEHICLE_TYPE_HELI)
	{
		if ( (vehicle.InheritsFromAutomobile() || vehicle.InheritsFromBike()) &&
			( IsPopTypeRandomNonPermanent(vehicle.PopTypeGet()) || (ped.GetMyVehicle() == &vehicle) || vehicle.m_nVehicleFlags.bNonAmbientVehicleMayBeUsedByGoToPointAnyMeans ) )
		{
			if(!CUpsideDownCarCheck::IsCarUpsideDown(&vehicleToSteal))
			{
				if(vehicle.CanBeDriven() && ped.GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanUseVehicles))
				{
					//Check that the vehicle isn't a cop car.
					if(!bIgnoreLawEnforcementVehicles || !vehicle.IsLawEnforcementVehicle())
					{
						//Check that the ped isn't friendly with the driver of the car and that the car driver isn't the player.
						//Don't try to steal the player car because if ped gets in the passenger seat and then shuffles across to
						//the driver seat we can't make the player get out the car to make space.
						if(0==vehicle.GetDriver() || (!vehicle.GetDriver()->PopTypeIsMission() && !vehicle.GetDriver()->IsPlayer() && !ped.GetPedIntelligence()->IsFriendlyWith(*vehicle.GetDriver()) && !CPedGroups::AreInSameGroup(&ped,vehicle.GetDriver())))
						{
							//If ped is in a group then check that nobody else in the group is in the car/entering the car/exiting the car.
							CPedGroup* pPedGroup=ped.GetPedsGroup();
							if(0==pPedGroup || !pPedGroup->IsAnyoneUsingCar(vehicle))     		    		
							{
								//Check that the vehicle is in a good state.
								if((g_fireMan.IsEntityOnFire(&vehicle)==false)&&(vehicle.GetHealth()>600 || bIgnoreVehicleHealth)&&(!vehicle.IsUpsideDown())&&(!vehicle.IsOnItsSide()))
								{
									//Check that there is a clear space at the front of the car.
									if(IsClearToDriveAway(vehicle))
									{
										return true;
									}//is clear to drive
								}//good state
							}//doesn't involve group using car
						}//no driver or not friends with driver
					}//isn't cop car
				}//isn't trailer
			}//isn't  upside down.
		}//car or bike and not mision car
	}//not plane or heli
	return false;
}

bool CCarEnterExit::IsClearToDriveAway(const CVehicle& vehicle)
{
	//    const CColModel& colModel=CModelInfo::GetColModel(vehicle.GetModelIndex());
	//    const spdAABB& colBox=colModel.GetBoundBox();
	//    const float fLength=colBox.m_max.y-colBox.m_min.y;
	const float fLength = vehicle.GetBaseModelInfo()->GetBoundingBoxMax().y - vehicle.GetBaseModelInfo()->GetBoundingBoxMin().y;
	const Vector3 vCentre = VEC3V_TO_VECTOR3(vehicle.GetTransform().GetPosition());
	Vector3 vDir=VEC3V_TO_VECTOR3(vehicle.GetTransform().GetB());
	vDir*=(0.5f*fLength+1.5f);

	INC_PEDAI_LOS_COUNTER;
	WorldProbe::CShapeTestProbeDesc probeDesc;
	WorldProbe::CShapeTestFixedResults<> probeResult;
	probeDesc.SetResultsStructure(&probeResult);
	probeDesc.SetStartAndEnd(vCentre+vDir, vCentre);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE);
	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
	{
		if(probeResult[0].GetHitInst() != vehicle.GetCurrentPhysicsInst())
		{
			return false;
		}
	}

	return true;
}  


void CCarEnterExit::MakeUndraggedPassengerPedsLeaveCar(const CVehicle& vehicle, const CPed* pDraggedPed, const CPed* pDraggingPed)
{
	for(s32 iSeat=0;iSeat<vehicle.GetSeatManager()->GetMaxSeats();iSeat++)
	{
		CPed* pPassenger=vehicle.GetSeatManager()->GetPedInSeat(iSeat);

		if(pPassenger && pPassenger!=pDraggedPed && !pPassenger->GetPedConfigFlag( CPED_CONFIG_FLAG_StayInCarOnJack ))
		{
			CEventPedEnteredMyVehicle event(pDraggingPed,&vehicle);
			pPassenger->GetPedIntelligence()->AddEvent(event);  			
		}
	}
}

void CCarEnterExit::MakeUndraggedDriverPedLeaveCar(const CVehicle& vehicle, const CPed& pedGettingIn)
{
	Assert(vehicle.GetDriver());
	CPed* pDriver=vehicle.GetDriver();
	CEventDraggedOutCar event(&vehicle,&pedGettingIn,true);
	pDriver->GetPedIntelligence()->AddEvent(event);  			
}


//*************************************************************************
//	GetCollidedDoorFromCollisionComponent
//	Helper function to return which door has been collided with, based on
//	the component index of the part of the vehicle returned by the physics.
//	This also must handle colliding with the window sections of the doors!

const CCarDoor *
CCarEnterExit::GetCollidedDoorFromCollisionComponent(const CVehicle * pVehicle, const int iHitComponent, const float fMinOpenThreshold)
{
	int d;
	int iDoorComponentIds[4];
	int iWindowComponentIds[4];
	static const eHierarchyId iDoorHierarchyIds[4] = { VEH_DOOR_DSIDE_F, VEH_DOOR_DSIDE_R, VEH_DOOR_PSIDE_F, VEH_DOOR_PSIDE_R };
	static const eHierarchyId iWindowHierarchyIds[4] = { VEH_WINDOW_LF, VEH_WINDOW_LR, VEH_WINDOW_RF, VEH_WINDOW_RR };

	for(d=0; d<4; d++)
	{
		iDoorComponentIds[d] = pVehicle->GetFragInst()->GetComponentFromBoneIndex(pVehicle->GetBoneIndex( iDoorHierarchyIds[d] ));
		iWindowComponentIds[d] = pVehicle->GetFragInst()->GetComponentFromBoneIndex(pVehicle->GetBoneIndex( iWindowHierarchyIds[d] ));
	}

	for(d=0; d<4; d++)
	{
		if(iHitComponent==iDoorComponentIds[d])
		{
			const CCarDoor * pDoor = pVehicle->GetDoorFromId( (eHierarchyId) iDoorHierarchyIds[d]);
			if(pDoor && pDoor->GetIsIntact(pVehicle) && pDoor->GetDoorRatio() >= fMinOpenThreshold)
				return pDoor;
		}
		else if(iHitComponent==iWindowComponentIds[d])
		{
			const CCarDoor * pDoor = pVehicle->GetDoorFromId( (eHierarchyId) iDoorHierarchyIds[d]);
			if(pDoor && pDoor->GetIsIntact(pVehicle) && pDoor->GetDoorRatio() >= fMinOpenThreshold)
				return pDoor;
		}
	}

	return NULL;
}

const CCarDoor*
CCarEnterExit::GetCollidedDoorBonnetBootFromCollisionComponent(const CVehicle * pVehicle, const int iHitComponent, const float fMinOpenThreshold)
{
	int d;
	int iDoorComponentIds[7];
	int iWindowComponentIds[6];
	// VEH_MOD_COLLISION_2 is the modded trunk/boot for Moonbeam2
	static const eHierarchyId iDoorHierarchyIds[7] = { VEH_DOOR_DSIDE_F, VEH_DOOR_DSIDE_R, VEH_DOOR_PSIDE_F, VEH_DOOR_PSIDE_R, VEH_BONNET, VEH_BOOT, VEH_MOD_COLLISION_2};
	static const eHierarchyId iWindowHierarchyIds[6] = { VEH_WINDOW_LF, VEH_WINDOW_LR, VEH_WINDOW_RF, VEH_WINDOW_RR, VEH_WINDSCREEN, VEH_WINDSCREEN_R };


	int iDoorCount = sizeof(iDoorHierarchyIds)/sizeof(iDoorHierarchyIds[0]);
	int iWindowCount = sizeof(iWindowHierarchyIds)/sizeof(iWindowHierarchyIds[0]);
	int iMaxCount = MAX(iDoorCount,iWindowCount);
	for(d=0; d<iMaxCount; d++)
	{
		if (d < iDoorCount)
			iDoorComponentIds[d] = pVehicle->GetFragInst()->GetComponentFromBoneIndex(pVehicle->GetBoneIndex( iDoorHierarchyIds[d] ));
		if (d < iWindowCount)
			iWindowComponentIds[d] = pVehicle->GetFragInst()->GetComponentFromBoneIndex(pVehicle->GetBoneIndex( iWindowHierarchyIds[d] ));
	}


	for(d=0; d<iMaxCount; d++)
	{
		if(d < iDoorCount && iHitComponent==iDoorComponentIds[d])
		{
			const CCarDoor * pDoor = pVehicle->GetDoorFromId( (eHierarchyId) iDoorHierarchyIds[d]);
			if(pDoor && pDoor->GetIsIntact(pVehicle) && pDoor->GetDoorRatio() >= fMinOpenThreshold)
				return pDoor;
		}
		else if(d < iWindowCount && iHitComponent==iWindowComponentIds[d])
		{
			const CCarDoor * pDoor = pVehicle->GetDoorFromId( (eHierarchyId) iDoorHierarchyIds[d]);
			if(pDoor && pDoor->GetIsIntact(pVehicle) && pDoor->GetDoorRatio() >= fMinOpenThreshold)
				return pDoor;
		}
	}

	return NULL;
}


const CWheel *
CCarEnterExit::GetCollidedWheelFromCollisionComponent(const CVehicle * pVehicle, const int iHitComponent)
{
	if(iHitComponent < 0)
	{
		return NULL;
	}

	for(int iWheelId = 0; iWheelId < pVehicle->GetNumWheels(); iWheelId++)
	{
		if(const CWheel *pWheel = pVehicle->GetWheel(iWheelId))
		{
			for(int iWheelBound = 0; iWheelBound < MAX_WHEEL_BOUNDS_PER_WHEEL; iWheelBound++)
			{
				if(pWheel->GetFragChild(iWheelBound) == iHitComponent)
				{
					return pWheel;
				}
			}
		}
	}

	return NULL;
}


eHierarchyId CCarEnterExit::DoorBoneIndexToHierarchyID(const CVehicle * pVehicle, const s32 iBoneIndex)
{
	if(pVehicle->GetBoneIndex(VEH_DOOR_DSIDE_F)==iBoneIndex)
		return VEH_DOOR_DSIDE_F;
	if(pVehicle->GetBoneIndex(VEH_DOOR_DSIDE_R)==iBoneIndex)
		return VEH_DOOR_DSIDE_R;
	if(pVehicle->GetBoneIndex(VEH_DOOR_PSIDE_F)==iBoneIndex)
		return VEH_DOOR_PSIDE_F;
	if(pVehicle->GetBoneIndex(VEH_DOOR_PSIDE_R)==iBoneIndex)
		return VEH_DOOR_PSIDE_R;
	return VEH_INVALID_ID;
}

int CCarEnterExit::GetDoorComponents(const CVehicle * pVehicle, const eHierarchyId iDoorId, int * pComponentIndices, const s32 iMaxNum)
{
	int iNumParts=0;
	eHierarchyId iIDs[MAX_DOOR_COMPONENTS];
	iIDs[iNumParts++]=iDoorId;

	switch(iDoorId)
	{
	case VEH_DOOR_DSIDE_F:
		iIDs[iNumParts++]=VEH_WINDOW_LF;
		break;
	case VEH_DOOR_DSIDE_R:
		iIDs[iNumParts++]=VEH_WINDOW_LR;
		break;
	case VEH_DOOR_PSIDE_F:
		iIDs[iNumParts++]=VEH_WINDOW_RF;
		break;
	case VEH_DOOR_PSIDE_R:
		iIDs[iNumParts++]=VEH_WINDOW_RR;
		break;
	default:
		return 0;
	}

	int i;
	for(i=0; i<iNumParts; i++)
	{
		if(iMaxNum >= 0 && i >= iMaxNum)	// optional breakout
			break;

		pComponentIndices[i] = pVehicle->GetFragInst()->GetComponentFromBoneIndex(pVehicle->GetBoneIndex( iIDs[i] ));
	}

	return i;
}

int CCarEnterExit::GetHeliComponents(CVehicle * pVehicle, int * pComponentIndices, const eHierarchyId iDoorId)
{
	int iStartIndex = 0;
	if(iDoorId != VEH_INVALID_ID)
	{
		iStartIndex = GetDoorComponents(pVehicle, iDoorId, pComponentIndices);
	}

	static const eHierarchyId iIDs[6] =
	{
		VEH_MISC_A, VEH_MISC_B, VEH_MISC_C, VEH_MISC_D, VEH_MISC_E, VEH_EXTRA_1
	};

	int iNum = iStartIndex;

	for(int i=0; i<6; i++)
	{
		const int iComponent = pVehicle->GetFragInst()->GetComponentFromBoneIndex(pVehicle->GetBoneIndex(iIDs[i]));

		if(iComponent >= 0)
		{
			pComponentIndices[iNum++] = iComponent;
		}
	}

	return iNum;
}

//***********************************************************************************************************
// CanDoorBePushedClosed
// Assuming that the ped is travelling from vPedPos to vTargetPos and has collided with the door, then
// function determines whether they can just continue walking since the door will close in that direction.
// This relies on the fact that all doors close in the same direction (hinged towards the front of the car).

bool CCarEnterExit::CanDoorBePushedClosed(CVehicle * pVehicle, const CCarDoor * UNUSED_PARAM(pDoor), const Vector3 & vPedPos, const Vector3 & vTargetPos)
{
	Vector3 vToTarget = vTargetPos - vPedPos;
	vToTarget.z = 0.0f;
	if(vToTarget.Mag2() < 0.01f)
		return true;

	Vector3 vCarForwards = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetB());
	vCarForwards.z = 0.0f;
	vCarForwards.Normalize();

	// When traveling from from to rear of vehicle, all doors can be trivially pushed shut
	vToTarget.Normalize();
	if(DotProduct(vToTarget, vCarForwards) < 0.0f)
		return true;

	// When heading in towards the vehicle from the side, we want to ignore collisions with doors
	// on that same side of the vehicle (unless we are moving towards the front of the vehicle
	// by some significant factor).
	Vector3 vCarRight = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetA());
	vCarRight.z = 0.0f;
	vCarRight.Normalize();

	const float fCentrePlaneDist = - DotProduct(vCarRight, VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()));
	const float fCarSide = DotProduct(vCarRight, vPedPos) + fCentrePlaneDist;

	static const float fAngEps = rage::Cosf( ( DtoR * 22.5f) );
	const float fDot = rage::Abs(DotProduct(vToTarget, vCarRight));

	if(fCarSide < 0.0f && fDot > fAngEps)
		return true;
	if(fCarSide > 0.0f && fDot < -fAngEps)
		return true;

	return false;
}
