//
// camera/cinematic/shot/camInVehicleWantedShot.cpp
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#include "CinematicInVehicleWantedShot.h"

#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"
#include "camera/cinematic/context/CinematicInVehicleWantedContext.h"
#include "camera/cinematic/camera/tracking/CinematicPositionCamera.h"

#include "peds/ped.h"
#include "peds/PedIntelligence.h"

#include "control/Replay/ReplayBufferMarker.h"
#include "game/Dispatch/RoadBlock.h"
#include "task/Combat/Subtasks/TaskVehicleCombat.h"
#include "task/Combat/TaskNewCombat.h"


INSTANTIATE_RTTI_CLASS(camPoliceCarMountedShot,0x86635317)
INSTANTIATE_RTTI_CLASS(camPoliceHeliMountedShot,0x8DD43F35)
INSTANTIATE_RTTI_CLASS(camPoliceInCoverShot,0x80F3F1F2)
INSTANTIATE_RTTI_CLASS(camPoliceExitVehicleShot,0x5FB1256B)
INSTANTIATE_RTTI_CLASS(camCinematicPoliceRoadBlockShot,0xAE43F82B)

CAMERA_OPTIMISATIONS()

camPoliceCarMountedShot::camPoliceCarMountedShot(const camBaseObjectMetadata& metadata)
: camBaseCinematicShot(metadata)
, m_Metadata(static_cast<const camCinematicPoliceCarMountedShotMetadata&>(metadata))
{
}

void camPoliceCarMountedShot::InitCamera()
{
	if(m_AttachEntity && m_LookAtEntity)
	{
		if(m_UpdatedCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))
		{
			camCinematicMountedCamera* pCam = static_cast<camCinematicMountedCamera*>(m_UpdatedCamera.Get()); 
			pCam->ShouldDisableControlHelper(true); 
			pCam->LookAt(m_LookAtEntity); 
			pCam->SetLookAtBehaviour(LOOK_AT_FOLLOW_PED_RELATIVE_POSITION);
			pCam->AttachTo(m_AttachEntity);
			pCam->SetShouldLimitAndTerminateForPitchAndHeadingLimits(m_Metadata.m_LimitAttachParentRelativePitchAndHeading, 
				m_Metadata.m_ShouldTerminateForPitchAndHeading, m_Metadata.m_InitialRelativePitchLimits, 
				m_Metadata.m_InitialRelativeHeadingLimits, m_Metadata.m_AttachParentRelativePitch, m_Metadata.m_AttachParentRelativeHeading); 
			
			pCam->OverrideLookAtDampingHelpers(m_Metadata.m_InVehicleLookAtDampingRef, m_Metadata.m_OnFootLookAtDampingRef); 
			pCam->SetCanUseLookAtDampingHelpers(true); 
			pCam->SetCanUseLookAheadHelper(m_Metadata.m_ShouldUseLookAhead);
			pCam->SetShouldTerminateForOcclusion(true); 
			pCam->SetShouldTerminateForDistance(true); 
		}
	}
}

u32 camPoliceCarMountedShot::GetCameraRef() const
{
	if(m_AttachEntity && m_AttachEntity->GetIsTypeVehicle())
	{	
		const CVehicle* pVehicle = static_cast<const CVehicle*>(m_AttachEntity.Get()); 

		const CVehicleModelInfo* targetVehicleModelInfo	= pVehicle->GetVehicleModelInfo();

		return targetVehicleModelInfo->GetBonnetCameraNameHash(); 

	}
	return 0;
}

bool camPoliceCarMountedShot::IsValid() const
{
	if(!camBaseCinematicShot::IsValidForGameState())
	{
		return false; 
	}

	if(m_AttachEntity && m_AttachEntity->GetIsTypeVehicle() && m_LookAtEntity)
	{
		return true; 
	}
	return false; 
}

const CEntity* camPoliceCarMountedShot::ComputeAttachEntity() const
{
	const CEntity* pEnt = NULL; 
	const CPed* pPed = NULL; 
	const CPed* followPed = camInterface::FindFollowPed();
	
	atVector<const CPed*> PolicePeds; 

	if(followPed && followPed->GetPedIntelligence())
	{
		const CEntityScannerIterator nearbyPedInterator = followPed->GetPedIntelligence()->GetNearbyPeds();
		for(const CEntity* pEnt = nearbyPedInterator.GetFirst(); pEnt; pEnt = nearbyPedInterator.GetNext())
		{
			const CPed* pThisPed = static_cast<const CPed*>(pEnt);
			
			if(pThisPed->GetPedType() == PEDTYPE_COP || pThisPed->GetPedType() == PEDTYPE_SWAT)
			{
				if(IsCopChasingInCar(pThisPed))
				{
					PolicePeds.PushAndGrow(pThisPed); 
				}
			}
		}
	}
	else
	{
		return NULL; 
	}

	if(PolicePeds.GetCount() == 1)
	{
		pPed = PolicePeds[0]; 
	}
	else
	{
		if(PolicePeds.GetCount() > 1)
		{
			pPed = PolicePeds[(fwRandom::GetRandomNumberInRange( 0, PolicePeds.GetCount()))];

		}
	}

	if(pPed)
	{
		CVehicle* pVehicle = pPed->GetVehiclePedInside(); 

		if(pVehicle && pVehicle->InheritsFromAutomobile())
		{
			pEnt = pVehicle; 
		}
	}

	return pEnt; 
}

bool camPoliceCarMountedShot::IsCopChasingInCar(const CPed* pPed) const
{
	if(pPed)
	{
		if(pPed->IsNetworkClone())
		{
			const CQueriableInterface* PedQueriableInterface = pPed->GetPedIntelligence()->GetQueriableInterface();
			if(PedQueriableInterface && PedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT))
			{	
				if(PedQueriableInterface->GetStateForTaskType(CTaskTypes::TASK_COMBAT) == CTaskCombat::State_PersueInVehicle)
				{
					return true;
				}
			}
		}
		else
		{
			CTask* pTask = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_VEHICLE_PERSUIT); 

			if(pTask)
			{
				CTaskVehiclePersuit* pTaskPursuit = static_cast<CTaskVehiclePersuit*>(pTask); 
				if(pTaskPursuit->GetState() == CTaskVehiclePersuit::State_chaseInCar)
				{
					return true; 
				}
			}
		}
	}
	return false; 
}

camPoliceHeliMountedShot::camPoliceHeliMountedShot(const camBaseObjectMetadata& metadata)
: camBaseCinematicShot(metadata)
, m_Metadata(static_cast<const camCinematicPoliceHeliMountedShotMetadata&>(metadata))
{
}

void camPoliceHeliMountedShot::InitCamera() 
{
	if(m_AttachEntity && m_LookAtEntity)
	{
		if(m_UpdatedCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))
		{
			camCinematicMountedCamera* pCam = static_cast<camCinematicMountedCamera*>(m_UpdatedCamera.Get()); 
			pCam->LookAt(m_LookAtEntity); 
			pCam->SetLookAtBehaviour(LOOK_AT_FOLLOW_PED_RELATIVE_POSITION);
			pCam->AttachTo(m_AttachEntity);
			pCam->SetCanUseLookAtDampingHelpers(true);
			pCam->SetCanUseLookAheadHelper(m_Metadata.m_ShouldUseLookAhead);
		}
	}
}


const CEntity* camPoliceHeliMountedShot::ComputeAttachEntity() const
{
	const CEntity* pEnt = NULL; 
	const CPed* pPed = NULL; 
	const CPed* followPed = camInterface::FindFollowPed();

	atVector<const CPed*> PolicePeds; 

	if(followPed && followPed->GetPedIntelligence())
	{
		const CEntityScannerIterator nearbyPedInterator = followPed->GetPedIntelligence()->GetNearbyPeds();
		for(const CEntity* pEnt = nearbyPedInterator.GetFirst(); pEnt; pEnt = nearbyPedInterator.GetNext())
		{
			const CPed* pThisPed = static_cast<const CPed*>(pEnt);

			if(pThisPed->GetPedType() == PEDTYPE_COP || pThisPed->GetPedType() == PEDTYPE_SWAT)
			{
				if(IsCopInHeli(pThisPed))
				{
					PolicePeds.PushAndGrow(pThisPed); 
				}
			}
		}
	}
	else
	{
		return NULL; 
	}
		
	if(PolicePeds.GetCount() == 1)
	{
		pPed = PolicePeds[0]; 
	}
	else
	{
		if(PolicePeds.GetCount() > 1)
		{
			pPed = PolicePeds[(fwRandom::GetRandomNumberInRange( 0, PolicePeds.GetCount()))];
		}
	}

	if(pPed)
	{
		CVehicle* pVehicle = pPed->GetVehiclePedInside(); 

		if(pVehicle && pVehicle->InheritsFromHeli())
		{
			pEnt = pVehicle; 
		}
	}

	return pEnt; 
}

bool camPoliceHeliMountedShot::IsCopInHeli(const CPed* pPed) const
{
	if(pPed)
	{
		if(pPed->IsNetworkClone())
		{
			const CQueriableInterface* PedQueriableInterface = pPed->GetPedIntelligence()->GetQueriableInterface();
			if(PedQueriableInterface && PedQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT))
			{	
				if(PedQueriableInterface->GetStateForTaskType(CTaskTypes::TASK_COMBAT) == CTaskCombat::State_PersueInVehicle)
				{
					return true;
				}
			}
		}
		else
		{
			CTask* pTask = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_HELI_COMBAT); 
			if(pTask)
			{
				return true; 
			}
		}
	}
	return false; 
}

bool camPoliceHeliMountedShot::IsValid() const
{
	if(!camBaseCinematicShot::IsValidForGameState())
	{
		return false; 
	}
	
	if(m_AttachEntity && m_AttachEntity->GetIsTypeVehicle() && m_LookAtEntity)
	{
		const CVehicle* pVehicle = static_cast<const CVehicle*>(m_AttachEntity.Get()); 

		if(pVehicle->InheritsFromHeli())
		{
			return true; 
		}
	}
	return false; 
}

camPoliceInCoverShot::camPoliceInCoverShot(const camBaseObjectMetadata& metadata)
: camBaseCinematicShot(metadata)
, m_Metadata(static_cast<const camCinematicPoliceInCoverShotMetadata&>(metadata))
{
}

void camPoliceInCoverShot::InitCamera()
{
	if(m_AttachEntity && m_LookAtEntity)
	{
		if(m_UpdatedCamera->GetIsClassId(camCinematicPositionCamera::GetStaticClassId()))
		{
				m_UpdatedCamera->LookAt(m_LookAtEntity);
				m_UpdatedCamera->AttachTo(m_AttachEntity);
				
				if(m_AttachEntity->GetIsTypePed())
				{
					Vector3 position; 
					float heading; 

					const CPed* ped = static_cast<const CPed*>(m_AttachEntity.Get());
					if(GetHeadingAndPositionOfCover(ped, position, heading))
					{
						((camCinematicPositionCamera*)m_UpdatedCamera.Get())->OverrideCamOrbitBasePosition(position, heading);
					}
				}
				((camCinematicPositionCamera*)m_UpdatedCamera.Get())->ComputeCamPositionRelativeToBaseOrbitPosition();
				((camCinematicPositionCamera*)m_UpdatedCamera.Get())->SetCanUseLookAtDampingHelpers(true);
				((camCinematicPositionCamera*)m_UpdatedCamera.Get())->SetCanUseLookAheadHelper(m_Metadata.m_ShouldUseLookAhead);
		}
	}
}

bool camPoliceInCoverShot::GetHeadingAndPositionOfCover(const CPed* pPed, Vector3 &position, float &heading)
{
	if(pPed)
	{
		CTask* pTask = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_COVER); 
		if(pTask)
		{
			CTaskInCover* pInCoverTask= static_cast<CTaskInCover*>(pTask); 
			if(pInCoverTask->GetState() > CTaskInCover::State_Start && pInCoverTask->GetState() < CTaskInCover::State_Finish)
			{
				position = pInCoverTask->GetCoverCoords();
				position.z += 1.0f;
				Vector3 worldForward(0.0f, 1.0f, 0.0f); 
				heading = camFrame::ComputeHeadingFromFront(pInCoverTask->GetCoverDirection()); 
				//heading = pInCoverTask->GetCoverDirection().AngleZ(worldForward );
				return true;
			}
		}
	}
	return false; 
}

camPoliceExitVehicleShot::camPoliceExitVehicleShot(const camBaseObjectMetadata& metadata)
: camBaseCinematicShot(metadata)
, m_Metadata(static_cast<const camCinematicPoliceExitVehicleShotMetadata&>(metadata))
{
}

void camPoliceExitVehicleShot::InitCamera()
{	
	if(m_AttachEntity && m_LookAtEntity)
	{
		if(m_UpdatedCamera->GetIsClassId(camCinematicPositionCamera::GetStaticClassId()))
		{
			m_UpdatedCamera->LookAt(m_LookAtEntity);
			m_UpdatedCamera->AttachTo(m_AttachEntity);
			((camCinematicPositionCamera*)m_UpdatedCamera.Get())->ComputeCamPositionRelativeToBaseOrbitPosition();
			((camCinematicPositionCamera*)m_UpdatedCamera.Get())->SetCanUseLookAtDampingHelpers(true);
			((camCinematicPositionCamera*)m_UpdatedCamera.Get())->SetCanUseLookAheadHelper(m_Metadata.m_ShouldUseLookAhead);
		}
	}
}

camCinematicPoliceRoadBlockShot::camCinematicPoliceRoadBlockShot(const camBaseObjectMetadata& metadata)
: camBaseCinematicShot(metadata)
, m_Metadata(static_cast<const camCinematicPoliceRoadBlockShotMetadata&>(metadata))
, m_RoadBlockPosition(VEC3_ZERO)
#if GTA_REPLAY
, m_iReplayRecordRoadBlock(0)
#endif
{
}

bool camCinematicPoliceRoadBlockShot::CanCreate()
{
	m_RoadBlockPosition = VEC3_ZERO; 
	MaxVehIcleHeightInRoadBlock = 2.0f; 

	if(m_LookAtEntity && m_LookAtEntity->GetIsTypeVehicle())
	{
		const CVehicle* pVehicle = static_cast<const CVehicle*>(m_LookAtEntity.Get()); 

		if(pVehicle && pVehicle->GetIsAircraft())
		{
			return false; 
		}
	}

	if(ComputeHaveRoadBlock(m_RoadBlockPosition))
	{
		float distance = (VEC3V_TO_VECTOR3(m_LookAtEntity->GetTransform().GetPosition()) - m_RoadBlockPosition).XYMag2(); 
		
		if( distance < m_Metadata.m_MaxDistanceToTrigger *  m_Metadata.m_MaxDistanceToTrigger)
		{
			return true; 
		}
	}

	return false; 
}

bool camCinematicPoliceRoadBlockShot::IsValid() const
{
	if(m_UpdatedCamera && m_LookAtEntity)
	{
		if(m_LookAtEntity->GetIsTypeVehicle())
		{
			const CVehicle* pVehicle = static_cast<const CVehicle*>(m_LookAtEntity.Get()); 

			if(pVehicle && pVehicle->GetIsAircraft())
			{
				return false; 
			}
		}
		
		float distance = (VEC3V_TO_VECTOR3(m_LookAtEntity->GetTransform().GetPosition()) - m_UpdatedCamera->GetFrame().GetPosition()).XYMag2(); 

		if(distance < m_Metadata.m_MinDistanceToTrigger *  m_Metadata.m_MinDistanceToTrigger  )
		{
			return false; 
		}
		
		Vector3 RoadBlockFront = m_RoadBlockPosition - m_UpdatedCamera->GetFrame().GetPosition(); 
		
		RoadBlockFront.NormalizeSafe(); 

		if(RoadBlockFront.Dot(m_UpdatedCamera->GetBaseFront()) < 0.0f)
		{
			return false; 
		}
		
	}
	
	return camBaseCinematicShot::IsValid();
}

void camCinematicPoliceRoadBlockShot::InitCamera()
{	
	if(m_LookAtEntity)	
	{
		if(m_UpdatedCamera->GetIsClassId(camCinematicPositionCamera::GetStaticClassId()))
		{
			m_UpdatedCamera->LookAt(m_LookAtEntity);
			m_RoadBlockPosition.z += MaxVehIcleHeightInRoadBlock; 
			
			Vector3 vDirection = VEC3V_TO_VECTOR3(m_LookAtEntity->GetTransform().GetPosition()) - m_RoadBlockPosition; 
			vDirection.NormalizeSafe(); 
			float heading = camFrame::ComputeHeadingFromFront(vDirection); 
			Matrix34 WorldMat; 
			camFrame::ComputeWorldMatrixFromFront(vDirection, WorldMat); 
			
			WorldMat.d = m_RoadBlockPosition; 

			Vector3 DistanceBackFromRoadBlock(0.0f, -12.0f, 0.0f); 
			
			WorldMat.Transform(DistanceBackFromRoadBlock); 
			
			((camCinematicPositionCamera*)m_UpdatedCamera.Get())->OverrideCamOrbitBasePosition(DistanceBackFromRoadBlock, heading);
			((camCinematicPositionCamera*)m_UpdatedCamera.Get())->ComputeCamPositionRelativeToBaseOrbitPosition();
			((camCinematicPositionCamera*)m_UpdatedCamera.Get())->SetShouldZoom(false);
			((camCinematicPositionCamera*)m_UpdatedCamera.Get())->SetShouldStartZoomed(false);
			((camCinematicPositionCamera*)m_UpdatedCamera.Get())->SetCanUseLookAtDampingHelpers(true);
			((camCinematicPositionCamera*)m_UpdatedCamera.Get())->SetCanUseLookAheadHelper(m_Metadata.m_ShouldUseLookAhead);
		}
	}
}


bool camCinematicPoliceRoadBlockShot::ComputeHaveRoadBlock(Vector3 &RoadBlockPos)
{
	if(!m_LookAtEntity)
	{
		return false; 
	}
	
	
	CRoadBlock::Pool* pool = CRoadBlock::GetPool(); 
	
	s32 i = pool->GetSize();
	
	float distanceMax = FLT_MAX; 
	Vector3 TempRoadBlockPos; 
	bool havePosition = false; 
	CRoadBlock* pSelectedRoadBlock = NULL; 

	while(i--)
	{
		CRoadBlock* pRoadBlock = pool->GetSlot(i);
		if(pRoadBlock)
		{
			if(pRoadBlock->GetVehCount() > 0 || pRoadBlock->GetPedCount() > 0)
			{
				TempRoadBlockPos = VEC3V_TO_VECTOR3(pRoadBlock->GetPosition());
				
				float distance = TempRoadBlockPos.Dist2(VEC3V_TO_VECTOR3(m_LookAtEntity->GetTransform().GetPosition())) ; 
				if(distance < distanceMax)
				{
					distanceMax = distance; 
					RoadBlockPos = VEC3V_TO_VECTOR3(pRoadBlock->GetPosition());
					havePosition = true; 
					pSelectedRoadBlock = pRoadBlock; 
				}
			}
		}
	}
	
	if(havePosition && pSelectedRoadBlock)
	{
		//Is the player driving towards the road block
		
		//compute the vector to the road blcok from the player
		Vector3 vToRoadBlock = RoadBlockPos - VEC3V_TO_VECTOR3(m_LookAtEntity->GetTransform().GetPosition()) ; 
		vToRoadBlock.NormalizeSafe(); 
		Vector3 PlayerFront = VEC3_ZERO; 
			
		camFrame::ComputeFrontFromHeadingAndPitch(m_LookAtEntity->GetTransform().GetHeading(), 0.0f, PlayerFront); 
		PlayerFront.NormalizeSafe(); 
		//if driving towards the road block then compute road block values else reject this road block
		if(vToRoadBlock.Dot(PlayerFront) > 0.0f) 
		{
			float VehicleHeightAboveGround = FLT_MAX; 
			for(int i =0; i<pSelectedRoadBlock->GetVehCount(); i++ )
			{
				CVehicle* pVehicle = pSelectedRoadBlock->GetVeh(i); 
				if(pVehicle)
				{
					//compute height for camera  above the ground
					if(pVehicle->GetBaseModelInfo()->GetBoundingBoxMax().z < VehicleHeightAboveGround)
					{
						MaxVehIcleHeightInRoadBlock = pVehicle->GetBaseModelInfo()->GetBoundingBoxMax().z; 

						VehicleHeightAboveGround = MaxVehIcleHeightInRoadBlock; 
#if GTA_REPLAY 
						//If the road block is close to the player then add a marker to record
						if((VEC3V_TO_VECTOR3(m_LookAtEntity->GetTransform().GetPosition()) - RoadBlockPos).Mag2() < rage::square(30))
						{
							if(m_iReplayRecordRoadBlock <= 0)
							{
								ReplayBufferMarkerMgr::AddMarker(2000,4000,IMPORTANCE_LOW);
								m_iReplayRecordRoadBlock = 4000;
							}
						}
#endif  //GTA_REPLAY
					}
				}
			}
		}
		else
		{
			havePosition = false; 
		}
	}

#if GTA_REPLAY 
	m_iReplayRecordRoadBlock -= fwTimer::GetTimeStepInMilliseconds();
#endif  //GTA_REPLAY

	return havePosition;
}