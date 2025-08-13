#include "TaskParkedVehicleScenario.h"

#include "fwmaths/angle.h"

#include "modelinfo/PedModelInfo.h"

#include "Peds/ped.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedCapsule.h"

#include "Scene/world/GameWorld.h"

#include "Task/Default/AmbientAnimationManager.h"
#include "Task/Default/TaskAmbient.h"

#include "Task/General/TaskBasic.h"

#include "Task/Movement/TaskNavMesh.h"
#include "Task/Movement/TaskSlideToCoord.h"

#include "Task/Scenario/ScenarioManager.h"
#include "Task/Scenario/info/ScenarioInfoManager.h"
#include "Task/Scenario/info/ScenarioTypes.h"

#include "Vehicles/vehicle.h"

AI_OPTIMISATIONS()

//-------------------------------------------------------------------------
// Parked Vehicle Scenario
//-------------------------------------------------------------------------

int CTaskParkedVehicleScenario::PickScenario(CVehicle* pVehicle, ParkedType parkedType)
{
	if( parkedType == PT_ParkedOnStreet )
	{
		s32 iNumScenarios = SCENARIOINFOMGR.GetScenarioCount(false);

		// This might be expensive going through all scenarios to pick a vehicle scenario
		for (s32 iSpecificScenario=0; iSpecificScenario<iNumScenarios; iSpecificScenario++)
		{
			const CScenarioInfo* pScenarioInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(iSpecificScenario);
			if (pScenarioInfo && pScenarioInfo->GetIsClass<CScenarioParkedVehicleInfo>())
			{
				taskAssert(dynamic_cast<const CScenarioParkedVehicleInfo*>(pScenarioInfo));

				const CScenarioParkedVehicleInfo* pPVInfo = static_cast<const CScenarioParkedVehicleInfo*>(pScenarioInfo);

				if (pPVInfo->GetVehicleScenarioType() == VSCENARIO_TYPE_BROKEN_DOWN_VEHICLE)
				{
					Vector3 vApproximatePosition = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
					// Scenarios associated with particular vehicles
					// Check the scenario times are correct and the population count for this scenario is within limits
					if(CanVehicleBeUsedForBrokenDownScenario(*pVehicle) &&
						CScenarioManager::CheckScenarioTimesAndProbability(NULL, iSpecificScenario) &&
						CScenarioManager::CheckScenarioPopulationCount(iSpecificScenario, vApproximatePosition, 1.0f, NULL))
					{
						return iSpecificScenario;
					}
				}
			}
		}
	}


	// 	if( parkedType == PT_ParkedOnStreet )
	// 	{
	// 		// Scenarios associated with particular vehicles
	// 		if( CheckScenarioTimesAndProbability( Vehicle_DeliveryDriver ) &&
	// 			(pVehicle->GetModelIndex() == MI_DELIVERY_TRUCK ||
	// 			pVehicle->GetModelIndex() == MI_DELIVERY_VAN ||
	// 			pVehicle->GetModelIndex() == MI_CAR_AMBULANCE) )
	// 		{
	// 			return Vehicle_DeliveryDriver;
	// 		}
	// 
	// 		if( pVehicle->GetDoorFromId(VEH_BONNET) && pVehicle->GetDoorFromId(VEH_BONNET)->GetIsIntact(pVehicle) &&
	// 			pVehicle->GetVehicleModelInfo()->GetApproximateBonnetHeight() < MIN_BONNET_HEIGHT_FOR_BONNET_CLIPS &&
	// 			!( pVehicle->pHandling->mFlags & MF_REVERSE_BONNET ) &&
	// 			CheckScenarioTimesAndProbability( Vehicle_InspectingBrokenDownVehicle ) )
	// 		{
	// 			return Vehicle_InspectingBrokenDownVehicle;
	// 		}
	// 
	// 		if( pVehicle->GetDoorFromId(VEH_BOOT) && pVehicle->GetDoorFromId(VEH_BOOT)->GetIsIntact(pVehicle) &&
	// 			!pVehicle->GetVehicleModelInfo()->GetFlag(FLAG_TAILGATE_TYPE_BOOT) &&
	// 			pVehicle->GetDefaultClipGroup() != CLIP_SET_VEH_VAN &&
	// 			CheckScenarioTimesAndProbability( Vehicle_LookingInBoot ) )
	// 		{
	// 			return Vehicle_LookingInBoot;
	// 		}
	// 
	// // 		if( CheckScenarioTimesAndProbability( Scenario_Leaning ) && fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < LEANING_AGAINST_CARS_PROB )
	// // 		{
	// // 			return Scenario_Leaning;
	// // 		}
	// 
	// 		if( CheckScenarioTimesAndProbability( Scenario_SmokingOutsideOffice ) && fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < SMOKING_AGAINST_CARS_PROB )
	// 		{
	// 			return Scenario_SmokingOutsideOffice;
	// 		}
	// 
	// 	}
	// 	else if( parkedType == PT_ParkedAtPetrolPumps )
	// 	{
	// 		if( pVehicle->GetDoorFromId(VEH_BONNET) && pVehicle->GetDoorFromId(VEH_BONNET)->GetIsIntact(pVehicle) &&
	// 			CheckScenarioTimesAndProbability( Vehicle_InspectingBrokenDownVehicle ) )
	// 		{
	// 			return Vehicle_InspectingBrokenDownVehicle;
	// 		}
	// 
	// 		if( pVehicle->GetDoorFromId(VEH_BOOT) && pVehicle->GetDoorFromId(VEH_BOOT)->GetIsIntact(pVehicle) &&
	// 				pVehicle->GetDefaultClipGroup() != CLIP_SET_VEH_VAN &&
	// 				!pVehicle->GetVehicleModelInfo()->GetFlag(FLAG_TAILGATE_TYPE_BOOT) &&
	// 				CheckScenarioTimesAndProbability( Vehicle_LookingInBoot ) )
	// 		{
	// 			return Vehicle_LookingInBoot;
	// 		}
	// 		
	// 		return Vehicle_SmokeThenDriveAway;
	// 	}
	// 	else if( parkedType == PT_ParkedOutsideMechanics )
	// 	{
	// 		if( pVehicle->GetDoorFromId(VEH_BONNET) && pVehicle->GetDoorFromId(VEH_BONNET)->GetIsIntact(pVehicle) )
	// 		{
	// 			ScenarioInfo* pInfo = CScenarioManager::GetScenarioInfo(Vehicle_InspectingBrokenDownVehicle);
	// 			if( CClock::IsTimeInRange(pInfo->m_startTime, pInfo->m_endTime ) )
	// 			{
	// 				return Vehicle_InspectingBrokenDownVehicle;
	// 			}
	// 		}
	// 	}

	return -1;
}

CTask*	CTaskParkedVehicleScenario::CreateScenarioTask(int scenarioTypeIndex, Vector3& vPedPos, float& fHeading, CVehicle* pVehicle)
{
	const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(scenarioTypeIndex);

	if (aiVerifyf(pScenarioInfo, "NULL Scenario Info in CTaskParkedVehicleScenario::SetupVehicleForScenario"))
	{
		aiAssert(dynamic_cast<const CScenarioParkedVehicleInfo*>(pScenarioInfo));
		const CScenarioParkedVehicleInfo* pPVInfo = static_cast<const CScenarioParkedVehicleInfo*>(pScenarioInfo);

		// Setup the vehicle depending on the scenario type
		if (pPVInfo->GetVehicleScenarioType() == VSCENARIO_TYPE_BROKEN_DOWN_VEHICLE)
		{
			// We could potentially data drive all this and get rid of the vehicle scenario type
			vPedPos = Vector3( 0.0f, pVehicle->GetBoundingBoxMax().y+PED_HUMAN_RADIUS+0.2f, 0.0f);
			vPedPos = pVehicle->TransformIntoWorldSpace(vPedPos);
			fHeading = pVehicle->GetTransform().GetHeading()+PI;
			return rage_new CTaskParkedVehicleScenario(scenarioTypeIndex, pVehicle);
		}
	}
	// 	else if( scenario == Vehicle_LookingInBoot )
	// 	{
	// 		vPedPos = Vector3( 0.0f, pVehicle->GetBoundingBoxMin().y-PED_HUMAN_RADIUS-0.2f, 0.0f);
	// 		pVehicle->TransformIntoWorldSpace(vPedPos);
	// 		fHeading = pVehicle->GetTransform().GetHeading();
	// 		if( fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < 0.5f )
	// 		{	
	// 			CCarDoor* pDSideDoor = pVehicle->GetDoorFromId(VEH_DOOR_DSIDE_F);
	// 			if(pDSideDoor)
	// 				pDSideDoor->SetTargetDoorOpenRatio(fwRandom::GetRandomNumberInRange(0.1f, 0.5f), CCarDoor::DRIVEN_NORESET);
	// 		}
	// 
	// 		CCarDoor* pBoot = pVehicle->GetDoorFromId(VEH_BOOT);
	// 		if(pBoot)
	// 			pBoot->SetTargetDoorOpenRatio(1.0f, CCarDoor::DRIVEN_AUTORESET);
	// 
	// 		pVehicle->SetCarDoorLocks(CARLOCK_UNLOCKED);
	// 		pVehicle->m_nVehicleFlags.bCarNeedsToBeHotwired = false;
	// 
	// 		return rage_new CTaskStationaryScenario(scenario, NULL, pVehicle, true, true);
	// 	}
	// 	else if( scenario == Vehicle_DeliveryDriver )
	// 	{
	// 		vPedPos = Vector3( 0.0f, pVehicle->GetBoundingBoxMin().y-PED_HUMAN_RADIUS-0.2f, 0.0f);
	// 		pVehicle->TransformIntoWorldSpace(vPedPos);
	// 		fHeading = pVehicle->GetTransform().GetHeading();
	// 		if( fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < 0.5f )
	// 		{	
	// 			CCarDoor* pDSideDoor = pVehicle->GetDoorFromId(VEH_DOOR_DSIDE_F);
	// 			if(pDSideDoor)
	// 				pDSideDoor->SetTargetDoorOpenRatio(fwRandom::GetRandomNumberInRange(0.1f, 0.5f), CCarDoor::DRIVEN_NORESET);
	// 		}
	// 
	// 		CCarDoor* pDSideRearDoor = pVehicle->GetDoorFromId(VEH_DOOR_DSIDE_R);
	// 		if(pDSideRearDoor)
	// 			pDSideRearDoor->SetTargetDoorOpenRatio(1.0f, CCarDoor::DRIVEN_NORESET);
	// 
	// 		CCarDoor* pPSideRearDoor = pVehicle->GetDoorFromId(VEH_DOOR_PSIDE_R);
	// 		if(pPSideRearDoor)
	// 			pPSideRearDoor->SetTargetDoorOpenRatio(1.0f, CCarDoor::DRIVEN_NORESET);
	// 
	// 		pVehicle->SetCarDoorLocks(CARLOCK_UNLOCKED);
	// 		pVehicle->m_nVehicleFlags.bCarNeedsToBeHotwired = false;
	// 
	// 		return rage_new CTaskStationaryScenario(scenario, NULL, pVehicle, true, true);
	// 	}
	// 	else if( scenario == Vehicle_SmokeThenDriveAway )
	// 	{
	// 		vPedPos = Vector3(pVehicle->GetBoundingBoxMin().x-PED_HUMAN_RADIUS-0.2f, 0.0f, 0.0f);
	// 		pVehicle->TransformIntoWorldSpace(vPedPos);
	// 		fHeading = pVehicle->GetTransform().GetHeading()+HALF_PI;
	// 
	// 		CCarDoor* pDoor = pVehicle->GetDoorFromId(VEH_DOOR_DSIDE_F);
	// 		if(pDoor)
	// 			pDoor->SetTargetDoorOpenRatio(1.0f, CCarDoor::DRIVEN_NORESET);
	// 
	// 		pVehicle->SetCarDoorLocks(CARLOCK_UNLOCKED);
	// 		pVehicle->m_nVehicleFlags.bCarNeedsToBeHotwired = false;
	// 
	// 		return rage_new CTaskComplexDrivingScenario( scenario, pVehicle );
	// // 			CTaskSequenceList* pSequence = new CTaskSequenceList();
	// // 			pSequence->AddTask( new CTaskStationaryScenario(scenario, NULL, pVehicle, true, true) );			// Get back in the van
	// // 			CTaskComplexNewGetInVehicle* pEnterTask = new CTaskComplexNewGetInVehicle( pVehicle, Seat_frontLeft);
	// // 			pEnterTask->SetPedMoveState(PEDMOVE_WALK);
	// // 			pSequence->AddTask( pEnterTask );
	// // 			return pSequence;
	// 	}
	// 	else if( scenario == Scenario_Leaning || Scenario_SmokingOutsideOffice )
	// 	{
	// 		static float EXTRA_OFFSET = PED_HUMAN_RADIUS * 0.5f;
	// 		bool bResultFound = false;
	// 		bool bSide = false;
	// 		CVehicleIntelligence::FindPavementRelativeToCar(pVehicle, bResultFound, bSide);
	// 		if( bSide || (!bResultFound && fwRandom::GetRandomTrueFalse()) )
	// 		{
	// 			vPedPos = Vector3(pVehicle->GetBoundingBoxMin().x-EXTRA_OFFSET, 0.0f, 0.0f);
	// 			pVehicle->TransformIntoWorldSpace(vPedPos);
	// 			fHeading = pVehicle->GetTransform().GetHeading()+HALF_PI;
	// 		}
	// 		else
	// 		{
	// 			vPedPos = Vector3(pVehicle->GetBoundingBoxMax().x+EXTRA_OFFSET, 0.0f, 0.0f);
	// 			pVehicle->TransformIntoWorldSpace(vPedPos);
	// 			fHeading = pVehicle->GetTransform().GetHeading()-HALF_PI;
	// 		}
	// 
	// 		return rage_new CTaskStationaryScenario(scenario, NULL, pVehicle, true, true);
	// 	}
	// 	else
	{
		return NULL;
	}
}

bool CTaskParkedVehicleScenario::CanVehicleBeUsedForBrokenDownScenario(const CVehicle& veh)
{
	const CCarDoor* pBonnet = veh.GetDoorFromId(VEH_BONNET);
	if(pBonnet && pBonnet->GetIsIntact(&veh))
	{
		CVehicleModelInfo* pModelInfo = veh.GetVehicleModelInfo();
		if(CScenarioManager::CanVehicleModelBeUsedForBrokenDownScenario(*pModelInfo))
		{

			// I experimented with the code below to detect bonnets that open the wrong way,
			// and mid/rear engined cars. Ultimately, I wasn't convinced that it's worth having
			// the code, when we could just use FLAG_NO_BROKEN_DOWN_SCENARIO.
			// Note: an alternative to FLAG_NO_BROKEN_DOWN_SCENARIO could also be to define a model
			// set for the vehicles that shouldn't get the scenario (or vice versa), and use something
			// like CScenarioInfo::m_BlockedModels. This would be more flexible, potentially allowing
			// different broken down vehicle scenarios for different cars, but also, more work.
#if 0
			if(pBonnet->GetOpenAmount() > pBonnet->GetClosedAmount())
			{
				Mat34V bonnetLocalMtrx;
				if(pBonnet->GetLocalMatrix(&veh, RC_MATRIX34(bonnetLocalMtrx)))
				{
					const ScalarV maxYV = RCC_VEC3V(pModelInfo->GetBoundingBoxMax()).GetY();
					const ScalarV minYV = RCC_VEC3V(pModelInfo->GetBoundingBoxMin()).GetY();
					const ScalarV centerYV = Average(minYV, maxYV);
					const ScalarV bonnetYV = bonnetLocalMtrx.GetCol3ConstRef().GetY();
					if(IsGreaterThanAll(bonnetYV, centerYV))
					{
						return true;
					}
				}
			}
#endif
			return true;
		}
	}
	return false;
}

CTaskParkedVehicleScenario::CTaskParkedVehicleScenario( s32 scenarioType, CVehicle* pScenarioVehicle, bool bWarpPed)
: CTaskScenario(scenarioType,NULL)
, m_vOriginalPedPos(Vector3::ZeroType)
, m_vOriginalVehiclePos(Vector3::ZeroType)
, m_fOriginalPedHeading(0.0f)
, m_fOriginalVehicleHeading(0.0f)
, m_bSetUpVehicle(false)
, m_bWarpPed(bWarpPed)
{
	m_pScenarioVehicle = pScenarioVehicle;
	taskAssertf(m_pScenarioVehicle, "NULL scenario vehicle");
	SetInternalTaskType(CTaskTypes::TASK_PARKED_VEHICLE_SCENARIO);
}

CTaskParkedVehicleScenario::~CTaskParkedVehicleScenario()
{
}

bool CTaskParkedVehicleScenario::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	if (GetSubTask())
	{
		return GetSubTask()->MakeAbortable(iPriority,pEvent);
	}
	return true;
}

CTask::FSM_Return CTaskParkedVehicleScenario::ProcessPreFSM()
{
	if(!m_pScenarioVehicle)
	{
		// We don't seem to have a vehicle, even though the constructor asserted that it
		// existed. Since there doesn't appear to be any other way for it to be cleared,
		// this probably means that the vehicle got destroyed, and we most likely wouldn't
		// want the scenario to continue.
		return FSM_Quit;
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskParkedVehicleScenario::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin

		FSM_State(State_Start)
		FSM_OnUpdate
		return Start_OnUpdate();

	FSM_State(State_PlayAmbients)
		FSM_OnEnter
		PlayAmbients_OnEnter();
	FSM_OnUpdate
		return PlayAmbients_OnUpdate();

	FSM_State(State_ReturnToPosition)
		FSM_OnEnter
		ReturnToPosition_OnEnter();
	FSM_OnUpdate
		return ReturnToPosition_OnUpdate();

	FSM_State(State_Finish)
		FSM_OnUpdate
		return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskParkedVehicleScenario::Start_OnUpdate()
{
	const CScenarioParkedVehicleInfo* pScenarioInfo = static_cast<const CScenarioParkedVehicleInfo*>(CScenarioManager::GetScenarioInfo(m_iScenarioIndex));

	CPed* pPed = GetPed();

	if (!taskVerifyf(pScenarioInfo, "NULL scenario info") || !m_pScenarioVehicle)
	{
		return FSM_Quit;
	}

	// Calculate where the ped should go to in order to begin ambient clips
	CalculateScenarioInitialPositionAndHeading();

	// If we haven't already setup the vehicle do so here
	if (!m_bSetUpVehicle)
	{
		if (!SetupVehicleForScenario(pScenarioInfo, m_pScenarioVehicle))
		{
			SetState(State_Finish);
			return FSM_Continue;
		}
	}

	if (m_bWarpPed)
	{
		m_bWarpPed = false;

		pPed->Teleport(m_vOriginalPedPos, m_fOriginalPedHeading, true);
		SetState(State_PlayAmbients);
		return FSM_Continue;
	}

	// If we're not near the start position, go there
	static dev_float sfMinDist = 1.0f;
	static dev_float sfMinHeadingDiff = 0.5f;
	if (VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()).Dist2(m_vOriginalPedPos) > sfMinDist || Abs(pPed->GetTransform().GetHeading() - m_fOriginalPedHeading) > sfMinHeadingDiff)
	{
		SetState(State_ReturnToPosition);
	}
	else
	{
		SetState(State_PlayAmbients);
	}

	return FSM_Continue;
}

void CTaskParkedVehicleScenario::PlayAmbients_OnEnter()
{
	taskAssertf(GetScenarioType()>=0,"NULL scenario type in CTaskParkedVehicleScenario::PlayAmbients_OnEnter");
	SetNewTask(rage_new CTaskAmbientClips(CTaskAmbientClips::Flag_PlayBaseClip | CTaskAmbientClips::Flag_PlayIdleClips, CScenarioManager::GetConditionalAnimsGroupForScenario(GetScenarioType())));
}

CTask::FSM_Return CTaskParkedVehicleScenario::PlayAmbients_OnUpdate()
{
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetFlag(aiTaskFlags::RestartCurrentState);
	}

	const CScenarioParkedVehicleInfo* pScenarioInfo = static_cast<const CScenarioParkedVehicleInfo*>(CScenarioManager::GetScenarioInfo(m_iScenarioIndex));
	
	if (taskVerifyf(pScenarioInfo, "NULL scenario info"))
	{
		if (pScenarioInfo->GetVehicleScenarioType() == VSCENARIO_TYPE_BROKEN_DOWN_VEHICLE && m_pScenarioVehicle)
		{
			//If the vehicle is no longer near the position we thought it was at ... end the scenario
			// If we're not near the start position, go there
			static dev_float sfMinDist = 0.5f;
			static dev_float sfMinHeadingDiff = 0.25f;
			if (VEC3V_TO_VECTOR3(m_pScenarioVehicle->GetTransform().GetPosition()).Dist2(m_vOriginalVehiclePos) > sfMinDist || Abs(m_pScenarioVehicle->GetTransform().GetHeading() - m_fOriginalVehicleHeading) > sfMinHeadingDiff)
			{
				//TODO: possible further logic here for triggering behavior based on the car haveing been moved ... 
				return FSM_Quit;
			}

			//Enter vehicle
			if (m_pScenarioVehicle->GetSeatManager()->CountPedsInSeats(true) != 0)
			{
				CPed* pPedInSeat = NULL;
				for (s32 seatIndex = 0; seatIndex < m_pScenarioVehicle->GetSeatManager()->GetMaxSeats(); seatIndex++)
				{
					if (m_pScenarioVehicle->GetSeatManager()->GetPedInSeat(seatIndex))
					{
						pPedInSeat = m_pScenarioVehicle->GetSeatManager()->GetPedInSeat(seatIndex);
						break;
					}
				}
				taskAssert(pPedInSeat);

				CEventAcquaintancePedHate event(pPedInSeat);
				GetPed()->GetPedIntelligence()->AddEvent(event);
			}

			//Stood on vehicle
			CPed * pPlayer = CGameWorld::FindLocalPlayer();
			if (pPlayer && pPlayer->GetGroundPhysical() && pPlayer->GetGroundPhysical()->GetIsTypeVehicle())
			{
				CVehicle *pOtherVehicle = static_cast<CVehicle*>(pPlayer->GetGroundPhysical());

				if (m_pScenarioVehicle == pOtherVehicle)
				{
					CEventAcquaintancePedHate event(pPlayer);
					GetPed()->GetPedIntelligence()->AddEvent(event);
				}
			}
		}
	}

	return FSM_Continue;
}

void CTaskParkedVehicleScenario::ReturnToPosition_OnEnter()
{
	CTaskSequenceList* pSequence = rage_new CTaskSequenceList();
	pSequence->AddTask( rage_new CTaskComplexControlMovement( rage_new CTaskMoveFollowNavMesh(MOVEBLENDRATIO_WALK, m_vOriginalPedPos)) );
	pSequence->AddTask( rage_new CTaskComplexControlMovement( rage_new CTaskMoveSlideToCoord(m_vOriginalPedPos, m_fOriginalPedHeading, 8.0f, PI, 2000)) );
	SetNewTask(rage_new CTaskUseSequence(*pSequence));
}

CTask::FSM_Return CTaskParkedVehicleScenario::ReturnToPosition_OnUpdate()
{
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_PlayAmbients);
	}
	return FSM_Continue;
}

bool CTaskParkedVehicleScenario::SetupVehicleForScenario(const CScenarioParkedVehicleInfo* pPVInfo, CVehicle* pVehicle)
{
	if (taskVerifyf(pPVInfo, "NULL scenario info"))
	{
		if (pPVInfo->GetVehicleScenarioType() == VSCENARIO_TYPE_BROKEN_DOWN_VEHICLE)
		{
			CScenarioManager::VehicleSetupBrokenDown(*pVehicle);
			m_bSetUpVehicle = true;
			return true;
		}
		else
		{
			taskAssertf(0, "Unhandled Vehicle Scenario");
		}
	}
	return false;
}

void CTaskParkedVehicleScenario::CalculateScenarioInitialPositionAndHeading()
{
	const CScenarioParkedVehicleInfo* pScenarioInfo = static_cast<const CScenarioParkedVehicleInfo*>(CScenarioManager::GetScenarioInfo(m_iScenarioIndex));
	if (aiVerifyf(pScenarioInfo, "NULL Scenario Info in CTaskParkedVehicleScenario::CalculateScenarioPosition"))
	{
		aiAssert(dynamic_cast<const CScenarioParkedVehicleInfo*>(pScenarioInfo));
		const CScenarioParkedVehicleInfo* pPVInfo = static_cast<const CScenarioParkedVehicleInfo*>(pScenarioInfo);

		// Setup the vehicle depending on the scenario type
		if (m_pScenarioVehicle && pPVInfo->GetVehicleScenarioType() == VSCENARIO_TYPE_BROKEN_DOWN_VEHICLE)
		{
			// We could potentially data drive all this and get rid of the vehicle scenario type
			if(GetPed())
			{
				m_vOriginalPedPos = Vector3( 0.0f, m_pScenarioVehicle->GetBoundingBoxMax().y+(GetPed()->GetCapsuleInfo()->GetHalfWidth())+0.2f, 0.0f);
			}
			else
			{
				m_vOriginalPedPos = Vector3( 0.0f, m_pScenarioVehicle->GetBoundingBoxMax().y+PED_HUMAN_RADIUS+0.2f, 0.0f);
			}
			m_vOriginalPedPos = m_pScenarioVehicle->TransformIntoWorldSpace(m_vOriginalPedPos);
			m_fOriginalPedHeading = fwAngle::LimitRadianAngle(m_pScenarioVehicle->GetTransform().GetHeading()+PI);
			m_vOriginalVehiclePos = VEC3V_TO_VECTOR3(m_pScenarioVehicle->GetTransform().GetPosition());
			m_fOriginalVehicleHeading = m_pScenarioVehicle->GetTransform().GetHeading();
		}
	}
}

#if !__FINAL
const char * CTaskParkedVehicleScenario::GetStaticStateName( s32 iState  )
{
	taskAssert( iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
	case State_Start:			return "State_Start";
	case State_PlayAmbients:	return "State_PlayAmbients";
	case State_ReturnToPosition:return "State_ReturnToPosition";
	case State_Finish:			return "State_Finish";
	default: taskAssert(0); 	
	}

	return "State_Invalid";
}
#endif
