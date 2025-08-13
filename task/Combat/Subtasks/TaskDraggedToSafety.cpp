// FILE :    TaskDraggedToSafety.cpp
// PURPOSE : Combat subtask in control of a ped being dragged to safety
// CREATED : 08-15-20011

// framework headers
#include "fwanimation/directorcomponentragdoll.h"

// Game headers
#include "event/EventDamage.h"
#include "Peds/ped.h"
#include "Peds/PedIntelligence.h"
#include "physics/physics.h"
#include "Task/Combat/Subtasks/TaskDraggedToSafety.h"
#include "Task/Combat/Subtasks/TaskDraggingToSafety.h"
#include "Task/Combat/TaskDamageDeath.h"
#include "Task/Physics/TaskNMDraggingToSafety.h"
#include "weapons/WeaponDamage.h"

AI_OPTIMISATIONS()

CTaskDraggedToSafety::CTaskDraggedToSafety(CPed* pDragger)
: m_pDragger(pDragger)
, m_bWasRunningInjuredOnGroundBehavior(false)
{
	SetInternalTaskType(CTaskTypes::TASK_DRAGGED_TO_SAFETY);
}

CTaskDraggedToSafety::~CTaskDraggedToSafety()
{
}

void CTaskDraggedToSafety::CleanUp()
{
	//Ensure the ped is valid.
	CPed* pPed = GetPed();
	if(pPed)
	{
		//Clear the no-collision entity.
		pPed->SetNoCollisionEntity(NULL);

		//Restore the death state.
		s32 iFlags = 0;
		if(!m_bWasRunningInjuredOnGroundBehavior)
		{
			//Disable the injured on ground behavior.
			pPed->GetPedIntelligence()->GetCombatBehaviour().ClearForcedInjuredOnGroundBehaviour();
			pPed->GetPedIntelligence()->GetCombatBehaviour().DisableInjuredOnGroundBehaviour();
			
			//Make sure the ped starts dead.
			iFlags |= CTaskDyingDead::Flag_startDead;
		}
		else
		{
			//Force the injured on ground behavior.
			pPed->GetPedIntelligence()->GetCombatBehaviour().ClearDisableInjuredOnGroundBehaviour();
			pPed->GetPedIntelligence()->GetCombatBehaviour().ForceInjuredOnGroundBehaviour(30.0f);
		}
		
		//Restore the death task.
		CDeathSourceInfo info(pPed->GetSourceOfDeath(), pPed->GetCauseOfDeath());
		CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY, rage_new CTaskDyingDead(&info, iFlags, pPed->GetTimeOfDeath()), true, E_PRIORITY_GIVE_PED_TASK, true);
		pPed->GetPedIntelligence()->AddEvent(event);
		
		//Check if the ped is not in ragdoll mode.
		if(!pPed->GetUsingRagdoll())
		{
			//Check if we can use a ragdoll.
			if(CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_DRAGGED_TO_SAFETY))
			{
				//Switch back to ragdoll.
				pPed->SwitchToRagdoll(*this);
			}
		}
	}
}

bool CTaskDraggedToSafety::ShouldAbort(const AbortPriority priority, const aiEvent* pEvent)
{
	//Allow immediate priority aborts.
	if(priority == ABORT_PRIORITY_IMMEDIATE)
	{
		return CTask::ShouldAbort(priority, pEvent);
	}

	return false;
}

CTask::FSM_Return CTaskDraggedToSafety::ProcessPreFSM()
{
	//Ensure the ped is valid.
	if(!GetPed())
	{
		return FSM_Quit;
	}
	
	//Ensure the dragger is valid.
	if(!GetDragger())
	{
		return FSM_Quit;
	}
	
	//Make sure the dragger is dragging this ped.
	if(!GetDragger()->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_DRAGGING_TO_SAFETY, true))
	{
		return FSM_Quit;
	}
	
	//Check if the ped is in ragdoll mode.
	if(GetPed()->GetRagdollState() == RAGDOLL_STATE_PHYS)
	{
		//Notify the ped that this task is in control of the ragdoll.
		GetPed()->TickRagdollStateFromTask(*this);
	}

	//Disable time slicing.
	GetPed()->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate);
	
	return FSM_Continue;
}

CTask::FSM_Return CTaskDraggedToSafety::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{	
	FSM_Begin
	
		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter();
			FSM_OnUpdate
				return Start_OnUpdate();
				
		FSM_State(State_Pickup)
			FSM_OnEnter
				Pickup_OnEnter();
			FSM_OnUpdate
				return Pickup_OnUpdate();
			FSM_OnExit
				Pickup_OnExit();
				
		FSM_State(State_Dragged)
			FSM_OnEnter
				Dragged_OnEnter();
			FSM_OnUpdate
				return Dragged_OnUpdate();
			FSM_OnExit
				Dragged_OnExit();
				
		FSM_State(State_Putdown)
			FSM_OnEnter
				Putdown_OnEnter();
			FSM_OnUpdate
				return Putdown_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
			
	FSM_End
}


void CTaskDraggedToSafety::Start_OnEnter()
{
	//Grab the ped.
	CPed* pPed = GetPed();
	
	//Check if the ped was running the injured on ground behavior.
	m_bWasRunningInjuredOnGroundBehavior = pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_NM_INJURED_ON_GROUND);
	
	//Make sure physics can activate.  Removing this will cause clips to not follow their move track.
	pPed->GetAnimatedInst()->SetInstFlag(phInst::FLAG_NEVER_ACTIVATE, false);
}

CTask::FSM_Return CTaskDraggedToSafety::Start_OnUpdate()
{
	//Keep the NM task across the state transition.
	SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
	
	//Sync the state.
	SyncState();

	CPed* pPed = GetPed();

	//Dragging is only allowed when the ped has a high-LOD ragdoll.
	if(!CTaskDraggingToSafety::ConvertPedToHighLodRagdoll(pPed))
	{
		return FSM_Quit;
	}

	if (!pPed->GetUsingRagdoll() )
	{
		if (CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_DRAGGED_TO_SAFETY))
		{
			//Make sure the ped is in ragdoll mode.
			pPed->SwitchToRagdoll(*this);
		}
		else
		{
			return FSM_Quit;
		}

	}
	
	return FSM_Continue;
}

void CTaskDraggedToSafety::Pickup_OnEnter()
{
	//Do not collide with the dragger ped.
	GetPed()->SetNoCollision(GetDragger(), NO_COLLISION_PERMENANT);

	//Choose the pickup clip.
	fwMvClipId clipId = ChoosePickupClip();
	
	//Blend from NM to the clip.
	static fwMvClipSetId s_ClipSetId("combat@drag_ped@",0x364F5832);
	BlendFromNMToAnimation(s_ClipSetId, clipId, -CTaskDraggingToSafety::GetTunables().m_SeparationPickup);
}

CTask::FSM_Return CTaskDraggedToSafety::Pickup_OnUpdate()
{
	//Sync the state.
	SyncState();

#if LAZY_RAGDOLL_BOUNDS_UPDATE
	GetDragger()->RequestRagdollBoundsUpdate();
	GetPed()->RequestRagdollBoundsUpdate();
#endif

	return FSM_Continue;
}

void CTaskDraggedToSafety::Pickup_OnExit()
{
	//Check if we are attached to the dragger.
	if(GetPed()->GetAttachParent() == GetDragger())
	{
		//Detach the ped from the dragger.  It was attached in the blend task.
		GetPed()->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS);
	}
}

void CTaskDraggedToSafety::Dragged_OnEnter()
{
	if(CTaskNMBehaviour::CanUseRagdoll(GetPed(), RAGDOLL_TRIGGER_DRAGGED_TO_SAFETY))
	{
		//Create the NM task.
		CTaskNMBehaviour* pTask = rage_new CTaskNMDraggingToSafety(GetPed(), GetDragger());
		
		//Do not switch to animated mode on abort.  This will cause the ped to pop in the air.
		pTask->SetFlag(CTaskNMBehaviour::DONT_SWITCH_TO_ANIMATED_ON_ABORT);
		
		//Start the task.
		SetNewTask(rage_new CTaskNMControl(1000, 50000, pTask, CTaskNMControl::ALL_FLAGS_CLEAR));
	}
}

CTask::FSM_Return CTaskDraggedToSafety::Dragged_OnUpdate()
{
	//Ensure the NM task is still running.
	if(!GetSubTask())
	{
		//For whatever reason NM has stopped, abort the task.
		return FSM_Quit;
	}

#if LAZY_RAGDOLL_BOUNDS_UPDATE
	GetDragger()->RequestRagdollBoundsUpdate();
	GetPed()->RequestRagdollBoundsUpdate();
#endif
	
	//Keep the NM task across the state transition.
	SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
	
	//Sync the state.
	SyncState();
	
	return FSM_Continue;
}

void CTaskDraggedToSafety::Dragged_OnExit()
{
	//Detach the ped.
	GetPed()->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS);
}

void CTaskDraggedToSafety::Putdown_OnEnter()
{
	//Blend from NM to the clip.
	static fwMvClipSetId s_ClipSetId("combat@drag_ped@",0x364F5832);
	BlendFromNMToAnimation(s_ClipSetId, CLIP_INJURED_PUTDOWN_PED, -CTaskDraggingToSafety::GetTunables().m_SeparationPutdown);
}

CTask::FSM_Return CTaskDraggedToSafety::Putdown_OnUpdate()
{
	//Sync the state.
	SyncState();
	
	return FSM_Continue;
}

void CTaskDraggedToSafety::BlendFromNMToAnimation(const fwMvClipSetId clipSetId, const fwMvClipId& clipId, const Vector3& vOffset)
{
	//Grab the ped.
	CPed* pPed = GetPed();
	
	//Calculate the clip matrix.
	Vector3 vPosition = GetDragger()->TransformIntoWorldSpace(vOffset);
	Matrix34 mClipMatrix = MAT34V_TO_MATRIX34(GetDragger()->GetMatrix());
	mClipMatrix.d = vPosition;

	// Try to find an accurate ground position for the capsule.
	const CBaseCapsuleInfo* pCapsuleInfo = pPed->GetCapsuleInfo();
	WorldProbe::CShapeTestFixedResults<> probeResult;
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetStartAndEnd(mClipMatrix.d, mClipMatrix.d - Vector3(0.0f, 0.0f, 1.2f));
	probeDesc.SetResultsStructure(&probeResult);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_VEHICLE_TYPE);
	probeDesc.SetExcludeEntity(pPed);
	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
	{
		mClipMatrix.d = probeResult[0].GetHitPosition() + Vector3(0.0f, 0.0f, pCapsuleInfo->GetGroundToRootOffset());
	}

	// Set the animated instance matrix based on our findings above.
	pPed->SetMatrix(mClipMatrix, false, false, false);

	if (!pPed->IsNetworkClone())
	{
		float fSwitchHeading = rage::Atan2f(-mClipMatrix.b.x, mClipMatrix.b.y);
		pPed->SetDesiredHeading(fSwitchHeading);
	}

	// Ready the ped's skeleton for the return to clip. Start by working out a transformation matrix from ragdoll to clip space.
	Matrix34 ragdollToClipMtx;
	ragdollToClipMtx.FastInverse(mClipMatrix);
	ragdollToClipMtx.DotFromLeft(RCC_MATRIX34(pPed->GetRagdollInst()->GetMatrix()));
	pPed->GetSkeleton()->Transform(RCC_MAT34V(ragdollToClipMtx));

	pPed->GetRagdollInst()->SetMatrix(RCC_MAT34V(mClipMatrix));
	CPhysics::GetLevel()->UpdateObjectLocation(pPed->GetRagdollInst()->GetLevelIndex());

	// Need to pose local matrices of skeleton because ragdoll has only been updating the global matrices.
	pPed->InverseUpdateSkeleton();

	// Blend in idle to stop any movement.
	pPed->StopAllMotion(true);

	if (pPed->GetAnimDirector())
	{
		fwAnimDirectorComponentRagDoll* componentRagDoll = pPed->GetAnimDirector()->GetComponent<fwAnimDirectorComponentRagDoll>();
		if(componentRagDoll)
		{
			// Actually make the switch to the animated instance.
			pPed->SwitchToAnimated(true, true, false, false, false);
			componentRagDoll->PoseFromRagDoll(*pPed->GetAnimDirector()->GetCreature());
			pPed->GetMovePed().SwitchToAnimated(0.25f, false);
		}
	}

	//Start the clip.
	int nFlags = APF_ISBLENDAUTOREMOVE;
	nFlags |= APF_ISFINISHAUTOREMOVE;
	CTask::StartClipBySetAndClip(pPed, clipSetId, clipId, nFlags, AP_MEDIUM, BONEMASK_ALL, INSTANT_BLEND_IN_DELTA, CClipHelper::TerminationType_OnFinish);
}

fwMvClipId CTaskDraggedToSafety::ChoosePickupClip() const
{
	//Ensure the task is valid.
	CTaskDraggingToSafety* pTask = static_cast<CTaskDraggingToSafety *>(GetDragger()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_DRAGGING_TO_SAFETY));
	if(!pTask)
	{
		return CLIP_ID_INVALID;
	}
	
	//Check the pickup direction.
	CTaskDraggingToSafety::PickupDirection nPickupDirection = pTask->GetPickupDirection();
	switch(nPickupDirection)
	{
		case CTaskDraggingToSafety::PD_Front:
		{
			return CLIP_INJURED_PICKUP_FRONT_PED;
		}
		case CTaskDraggingToSafety::PD_Back:
		{
			return CLIP_INJURED_PICKUP_BACK_PED;
		}
		case CTaskDraggingToSafety::PD_SideLeft:
		{
			return CLIP_INJURED_PICKUP_SIDE_LEFT_PED;
		}
		case CTaskDraggingToSafety::PD_SideRight:
		{
			return CLIP_INJURED_PICKUP_SIDE_RIGHT_PED;
		}
		default:
		{
			taskAssertf(false, "Unknown pickup direction: %d.", nPickupDirection);
			return CLIP_ID_INVALID;
		}
	}
}

void CTaskDraggedToSafety::SyncState()
{
	//Ensure the task is valid.
	CTaskDraggingToSafety* pTask = static_cast<CTaskDraggingToSafety *>(GetDragger()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_DRAGGING_TO_SAFETY));
	if(!pTask)
	{
		return;
	}
	
	//Grab the state.
	s32 iState = pTask->GetState();
	
	//Ignore states before pickup.
	if(iState < CTaskDraggingToSafety::State_Pickup)
	{
		return;
	}
	
	//Kill this task after putdown.
	if(iState > CTaskDraggingToSafety::State_Putdown)
	{
		SetState(State_Finish);
		return;
	}

	//Calculate the desired state.
	s32 iDesiredState = State_Finish;
	switch(iState)
	{
		case CTaskDraggingToSafety::State_Pickup:
		{
			iDesiredState = State_Pickup;
			break;
		}
		case CTaskDraggingToSafety::State_Dragging:
		case CTaskDraggingToSafety::State_Adjust:
		{
			iDesiredState = State_Dragged;
			break;
		}
		case CTaskDraggingToSafety::State_Putdown:
		{
			iDesiredState = State_Putdown;
			break;
		}
		default:
		{
			taskAssertf(false, "Unknown state: %d.", iState);
			return;
		}
	}
	
	//Ensure the state does not match.
	if(iDesiredState == GetState())
	{
		return;
	}
	
	//Set the state.
	SetState(iDesiredState);
}

#if !__FINAL
void CTaskDraggedToSafety::Debug() const
{
#if DEBUG_DRAW
	//Add a marker above the dragged ped's head.
	grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()) + Vector3(0.0f, 0.0f, 1.0f), 0.1f, Color_purple);
#endif
}
#endif // !__FINAL

#if !__FINAL
const char * CTaskDraggedToSafety::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Start&&iState<=State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Pickup",
		"State_Dragged",
		"State_Putdown",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL
