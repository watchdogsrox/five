//
// Task/Physics/TaskTryToGrabVehicleDoor.cpp
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

////////////////////////////////////////////////////////////////////////////////

#include "fragmentnm/nm_channel.h "

// Game Headers
#include "Event/Events.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "task/Movement/TaskIdle.h"
#include "task/Movement/TaskGetUp.h"
#include "Task/Physics/TaskAnimatedAttach.h"
#include "Task/Physics/TaskNMBalance.h"
#include "Task/Physics/TaskNMJumpRollFromRoadVehicle.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskTryToGrabVehicleDoor.h"
#include "Vehicles/Vehicle.h"
#include "Vehicles/Metadata/VehicleEntryPointInfo.h"
#include "pharticulated/articulatedcollider.h"
#include "phbound/boundcomposite.h"

////////////////////////////////////////////////////////////////////////////////

AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////

// Statics
CTaskTryToGrabVehicleDoor::Tunables CTaskTryToGrabVehicleDoor::ms_Tunables;

IMPLEMENT_VEHICLE_TASK_TUNABLES(CTaskTryToGrabVehicleDoor, 0xffb07e0e);

////////////////////////////////////////////////////////////////////////////////

bool CTaskTryToGrabVehicleDoor::IsTaskValid(CVehicle* pVehicle, CPed* pPed,  s32 iEntryPointIndex)
{
	if (!pVehicle || !pVehicle->IsEntryIndexValid(iEntryPointIndex))
		return false;
	if (!pPed || !CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_VEHICLE_GRAB))
		return false;

	return true;
}

////////////////////////////////////////////////////////////////////////////////

CTaskTryToGrabVehicleDoor::CTaskTryToGrabVehicleDoor(CVehicle* pVehicle, s32 iEntryPointIndex)
: m_pVehicle(pVehicle)
, m_iEntryPointIndex(iEntryPointIndex)
{
	SetInternalTaskType(CTaskTypes::TASK_TRY_TO_GRAB_VEHICLE_DOOR);
}

////////////////////////////////////////////////////////////////////////////////

CTaskTryToGrabVehicleDoor::~CTaskTryToGrabVehicleDoor()
{

}

////////////////////////////////////////////////////////////////////////////////

void CTaskTryToGrabVehicleDoor::CleanUp()
{

}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskTryToGrabVehicleDoor::ProcessPreFSM()
{
	if (!IsTaskValid(m_pVehicle, GetPed(), m_iEntryPointIndex))
		return FSM_Quit;

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskTryToGrabVehicleDoor::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();	

		FSM_State(State_GrabVehicleDoor)
			FSM_OnEnter
				return GrabVehicleDoor_OnEnter();
			FSM_OnUpdate
				return GrabVehicleDoor_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskTryToGrabVehicleDoor::Start_OnUpdate()
{
    CPed *pPed = GetPed();

    if(pPed->IsNetworkClone())
    {
        if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_NM_CONTROL) &&
           pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_NM_BALANCE))
        {
            SetState(State_GrabVehicleDoor);
        }
    }
    else
    {
	    SetState(State_GrabVehicleDoor);
    }
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskTryToGrabVehicleDoor::GrabVehicleDoor_OnEnter()
{
	const CEntryExitPoint* pEntryPoint = m_pVehicle->GetEntryExitPoint(m_iEntryPointIndex);	
	//@@: location CTASKTRYTOGRABVEHICLEDOOR_GRABVEHICLEDOOR_ONENTER
	const CVehicleEntryPointInfo* pEntryInfo = m_pVehicle->GetVehicleModelInfo()->GetEntryPointInfo(m_iEntryPointIndex);
	
	if (pEntryPoint && pEntryInfo)
	{
		s32 iDoorHandleBoneIndex = pEntryPoint->GetDoorHandleBoneIndex();
		if (iDoorHandleBoneIndex > -1)
		{
            CPed *pPed = GetPed();
            if(pPed->IsNetworkClone())
            {
                CTaskNMControl* pTaskNMControl = SafeCast(CTaskNMControl, pPed->GetPedIntelligence()->CreateCloneTaskForTaskType(CTaskTypes::TASK_NM_CONTROL));

                pTaskNMControl->SetGetGrabParametersCallback(CTaskTryToGrabVehicleDoor::SetupBalanceGrabHelper, this);

                SetNewTask(pTaskNMControl);
            }
            else
            {
				nmDebugf2("[%u] Adding nm balance task:%s(%p) Trying to grab vehicle door.", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed);
                CTaskNMBalance* pTaskNMBalance = rage_new CTaskNMBalance(ms_Tunables.m_MinGrabTime, ms_Tunables.m_MaxGrabTime, NULL, 0);

                // don't grab if this is a player, just balance
			    if (!CTaskNMBehaviour::ms_bDisableBumpGrabForPlayer || !GetPed()->IsPlayer())
			    {
				    SetupBalanceGrabHelper(this, pTaskNMBalance);
			    }

                CTaskNMControl* pTaskNMControl = rage_new CTaskNMControl(ms_Tunables.m_MinGrabTime, ms_Tunables.m_MaxGrabTime, pTaskNMBalance, CTaskNMControl::DO_BLEND_FROM_NM);

			    SetNewTask(pTaskNMControl);
            }

			//rage::phConstraintDistance::Params distanceConstraint;
			//distanceConstraint.instanceA = GetPed()->GetRagdollInst();
			//distanceConstraint.instanceB = m_pVehicle->GetCurrentPhysicsInst();
			//distanceConstraint.componentA = (rage::u16) RAGDOLL_HAND_RIGHT;
			//distanceConstraint.componentB = (rage::u16) grabComponent;
			//distanceConstraint.worldAnchorA = VECTOR3_TO_VEC3V(rightHandMat.d);
			//distanceConstraint.worldAnchorB = VECTOR3_TO_VEC3V(doorHandleGlobalMtx.d);
			//distanceConstraint.minDistance = 0.0f;
			//distanceConstraint.maxDistance = 0.01f;
			//distanceConstraint.allowedPenetration = 0.0f;
			//PHCONSTRAINT->Insert(distanceConstraint);
		}
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskTryToGrabVehicleDoor::GrabVehicleDoor_OnUpdate()
{
	//static float massMultHand = 3.0f;
	//static float massMultArm = 2.0f;
	if (GetIsSubtaskFinished(CTaskTypes::TASK_NM_CONTROL))
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

	// Keep the car articulated while a ragdoll is constrained to a swinging door.
	if (m_pVehicle && m_pVehicle->GetCollider())
		((phArticulatedCollider*)m_pVehicle->GetCollider())->SetDenyColliderSimplificationFrames(2);

	return FSM_Continue;
}
//////////////////////////////////////////////////////////////////////////
bool CTaskTryToGrabVehicleDoor::IsWithinGrabbingDistance(const CPed* pPed, const CVehicle* pVehicle, s32 iEntryPointIndex)
{
	taskAssert(pPed);
	taskAssert(pVehicle);
	taskAssert(pVehicle->GetVehicleModelInfo());
	const CEntryExitPoint* pEntryPoint = pVehicle->GetEntryExitPoint(iEntryPointIndex);
	const CVehicleEntryPointInfo* pEntryInfo = pVehicle->GetVehicleModelInfo()->GetEntryPointInfo(iEntryPointIndex);

	if (pEntryPoint && pEntryInfo)
	{
		s32 iDoorHandleBoneIndex = pEntryPoint->GetDoorHandleBoneIndex();
		if (iDoorHandleBoneIndex > -1)
		{
			// Get the world handle position
			Matrix34 doorHandleGlobalMtx;
			pVehicle->GetGlobalMtx(iDoorHandleBoneIndex, doorHandleGlobalMtx);
			Vector3 vGrabPos = doorHandleGlobalMtx.d;
			Vector3 vHandPos;

			if (pEntryInfo->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_LEFT)
				vHandPos = pPed->GetBonePositionCached(BONETAG_L_HAND);
			else
				vHandPos = pPed->GetBonePositionCached(BONETAG_R_HAND);

			if ((vGrabPos - vHandPos).Mag2()<(ms_Tunables.m_MaxHandToHandleDistance*ms_Tunables.m_MaxHandToHandleDistance))
			{
				return true;
			}

		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskTryToGrabVehicleDoor::SetupBalanceGrabHelper(aiTask *pTask, aiTask *pTaskNM)
{
    if(pTask   && (pTask->GetTaskType() == CTaskTypes::TASK_TRY_TO_GRAB_VEHICLE_DOOR) &&
       pTaskNM && (pTaskNM->GetTaskType() == CTaskTypes::TASK_NM_BALANCE))
    {
        CTaskTryToGrabVehicleDoor *pTaskTryToGrabVehicleDoor = SafeCast(CTaskTryToGrabVehicleDoor, pTask);
        CTaskNMBalance            *pTaskNMBalance            = SafeCast(CTaskNMBalance,            pTaskNM);

        CVehicle *pVehicle = pTaskTryToGrabVehicleDoor->m_pVehicle;
		taskAssert(pVehicle);
		taskAssert(pVehicle->GetVehicleModelInfo());
        const CEntryExitPoint* pEntryPoint = pVehicle->GetEntryExitPoint(pTaskTryToGrabVehicleDoor->m_iEntryPointIndex);
	    const CVehicleEntryPointInfo* pEntryInfo = pVehicle->GetVehicleModelInfo()->GetEntryPointInfo(pTaskTryToGrabVehicleDoor->m_iEntryPointIndex);

	    if (pEntryPoint && pEntryInfo)
	    {
		    s32 iDoorHandleBoneIndex = pEntryPoint->GetDoorHandleBoneIndex();
		    if (iDoorHandleBoneIndex > -1)
		    {
                // Keep the car articulated while a ragdoll is constrained to a swinging door.
				if (pVehicle->GetCollider())
					((phArticulatedCollider*)pVehicle->GetCollider())->SetDenyColliderSimplificationFrames(2);

	            // Get the world handle position
	            Matrix34 doorHandleGlobalMtx;
	            pVehicle->GetGlobalMtx(iDoorHandleBoneIndex, doorHandleGlobalMtx);
	            Vector3 vGrabPos = doorHandleGlobalMtx.d;

	            // Get the door component
	            CGrabHelper* grabHelper = pTaskNMBalance->GetGrabHelper();
				fragInst* pFragInst = pVehicle->GetFragInst();
				taskAssert(pFragInst);
				if (grabHelper && pVehicle->GetCurrentPhysicsInst() && pFragInst)
				{
					int grabComponent = pFragInst->GetControllingComponentFromBoneIndex(iDoorHandleBoneIndex);
					if (Verifyf(grabComponent >= 0, "CTaskTryToGrabVehicleDoor::GrabVehicleDoor_OnEnter() - component not found"))
						grabHelper->SetGrabTarget_BoundIndex(grabComponent);

					// Transform the handle position into bound space
					taskAssert(pFragInst->GetCached());
					phBoundComposite *carBound = pFragInst->GetCacheEntry()->GetBound();
					Matrix34 doorMat;
					doorMat.Dot(RCC_MATRIX34(carBound->GetCurrentMatrix(grabComponent)), RCC_MATRIX34(pVehicle->GetCurrentPhysicsInst()->GetMatrix()));
					doorMat.Inverse();
					doorMat.Transform(vGrabPos);

					if (pEntryInfo->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_LEFT)
						grabHelper->SetGrabTarget(pVehicle, NULL, NULL, &vGrabPos, NULL, NULL, NULL);
					else
						grabHelper->SetGrabTarget(pVehicle, &vGrabPos, NULL, NULL, NULL, NULL, NULL);

					static atHashString s_GrabVehicleDoorSet("GrabVehicleDoor");
					static atHashString s_GrabVehicleDoorBalanceFailedSet("GrabVehicleDoorOnBalanceFailed");
					grabHelper->SetInitialTuningSet(s_GrabVehicleDoorSet);
					grabHelper->SetBalanceFailedTuningSet(s_GrabVehicleDoorBalanceFailedSet);
				}
			}
        }
    }
}

bool CTaskTryToGrabVehicleDoor::AddGetUpSets( CNmBlendOutSetList& sets, CPed* pPed )
{
	//Check if the ped is valid.
	if(pPed)
	{
		// get the target vehicle
		if (m_pVehicle)
		{
			CAITarget target(m_pVehicle);
			sets.SetMoveTask(rage_new CTaskMoveFaceTarget(target));
			sets.Add(CTaskGetUp::ShouldUseInjuredGetup(pPed) ? NMBS_INJURED_STANDING : NMBS_STANDING);
			sets.Add(CTaskGetUp::PickStandardGetupSet(pPed));
			return true;
		}
	}

	// no custom blend out set specified. Use the default.
	return false;
}

////////////////////////////////////////////////////////////////////////////////
