// FILE :    TaskDraggingToSafety.cpp
// PURPOSE : Combat subtask in control of dragging a ped to safety
// CREATED : 08-15-20011

// Rage headers
#include "math/angmath.h"

// Game headers
#include "animation/EventTags.h"
#include "Peds/PedIntelligence.h"
#include "Peds/ped.h"
#include "physics/gtaInst.h"
#include "Task/Animation/TaskAnims.h"
#include "Task/Combat/TaskCombat.h"
#include "Task/Combat/Subtasks/TaskDraggedToSafety.h"
#include "Task/Combat/Subtasks/TaskDraggingToSafety.h"
#include "Task/Movement/TaskGotoPoint.h"
#include "Task/Movement/TaskNavMesh.h"
#include "task/Movement/TaskSlideToCoord.h"
#include "task\Scenario\Info\ScenarioInfoManager.h"
#include "task\Scenario\Types\TaskUseScenario.h"
#include "task/Weapons/TaskSwapWeapon.h"
#include "vfx/misc/Fire.h"

AI_OPTIMISATIONS()

namespace
{
	struct DraggingCoverPointData
	{
		Vector3 m_vPosition;
		Vector3 m_vTargetPosition;
		CPed*	m_pPed;
		CPed*	m_pDragged;
		bool	m_bUseTargetPosition;
	};
}

////////////////////////////////////////////////////////////////////////////////
// CTaskDraggingToSafety
////////////////////////////////////////////////////////////////////////////////

CTaskDraggingToSafety::Tunables CTaskDraggingToSafety::sm_Tunables;
IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskDraggingToSafety, 0x5816165e);

CTaskDraggingToSafety::CTaskDraggingToSafety(CPed* pDragged, const CPed* pPrimaryTarget, fwFlags8 uConfigFlags)
: m_vPositionToDragTo(V_ZERO)
, m_nPickupDirection(PD_Invalid)
, m_HeadTimer(sm_Tunables.m_LookAtUpdateTime)
, m_pDragged(pDragged)
, m_pPrimaryTarget(pPrimaryTarget)
, m_pCoverPoint(NULL)
, m_fTimeSinceLastCoverPointSearch(0.0f)
, m_fMoveBlendRatioForApproach(MOVEBLENDRATIO_RUN)
, m_fTimeWeHaveBeenObstructed(0.0f)
, m_uConfigFlags(uConfigFlags)
, m_uRunningFlags(0)
{
	SetInternalTaskType(CTaskTypes::TASK_DRAGGING_TO_SAFETY);
}

CTaskDraggingToSafety::~CTaskDraggingToSafety()
{
}

void CTaskDraggingToSafety::DragToPosition(Vec3V_In vPositionToDragTo)
{
	//Set the position to drag to.
	m_vPositionToDragTo = vPositionToDragTo;
	
	//Set the flag.
	m_uRunningFlags.SetFlag(RF_HasPositionToDragTo);
}

aiTask* CTaskDraggingToSafety::Copy() const
{
	//Create the task.
	CTaskDraggingToSafety* pTask = rage_new CTaskDraggingToSafety(GetDragged(), GetPrimaryTarget(), m_uConfigFlags);
	
	//Check if we have a position to drag to.
	if(m_uRunningFlags.IsFlagSet(RF_HasPositionToDragTo))
	{
		//Drag to the position.
		pTask->DragToPosition(m_vPositionToDragTo);
	}
	
	//Set the move blend ratio for approach.
	pTask->SetMoveBlendRatioForApproach(m_fMoveBlendRatioForApproach);

	return pTask;
}

void CTaskDraggingToSafety::CleanUp()
{
	//Clear the no-collision entity.
	GetPed()->SetNoCollisionEntity(NULL);

	//Ensure the dragged ped is valid.
	if(GetDragged())
	{
		//Check if this task was responsible for dragging the ped to safety.
		if(m_uRunningFlags.IsFlagSet(RF_IsDraggingToSafety))
		{
			//Note that the ped is no longer being dragged to safety.
			GetDragged()->SetPedConfigFlag(CPED_CONFIG_FLAG_IsBeingDraggedToSafety, false);
			
			//Clear the drag flag.
			m_uRunningFlags.ClearFlag(RF_IsDraggingToSafety);
		}
	}
	
	//If we are playing any clips, do a slow blend out.
	StopClip(SLOW_BLEND_OUT_DELTA);
	
	//Clear the cover point.
	ClearCoverPoint();
}

bool CTaskDraggingToSafety::ShouldAbort(const AbortPriority priority, const aiEvent* pEvent)
{
	//Keep track of whether the task should abort.
	bool bAbort = false;
	
	//Check if the event is valid.
	if(pEvent)
	{
		//Check the event type.
		switch(((CEvent *)pEvent)->GetEventType())
		{
			case EVENT_DAMAGE:
			case EVENT_ON_FIRE:
			{
				//If the ped is shot, stop the drag.
				bAbort = true;
				
				break;
			}
			default:
			{
				break;
			}
		}
	}
	
	//Allow immediate priority aborts.
	if(priority == ABORT_PRIORITY_IMMEDIATE)
	{
		bAbort = true;
	}
	
	//If we are going to abort anyways, allow the abort.
	if(m_uRunningFlags.IsFlagSet(RF_ShouldQuit))
	{
		bAbort = true;
	}
	
	if(!bAbort)
	{
		return false;
	}
	else
	{
		return CTask::ShouldAbort(priority, pEvent);
	}
}

CTask::FSM_Return CTaskDraggingToSafety::ProcessPreFSM()
{
	//Ensure the ped is valid.
	CPed* pPed = GetPed();
	if(!pPed)
	{
		return FSM_Quit;
	}
	
	//Ensure the ped has not been injured themselves.
	if(pPed->IsInjured())
	{
		return FSM_Quit;
	}
	
	//Ensure the dragged ped is valid.
	CPed* pDragged = GetDragged();
	if(!pDragged)
	{
		return FSM_Quit;
	}
	
	//Check if the dragged task should be active.
	bool bMustDraggedTaskBeActive = ((GetState() == State_Pickup) && (GetTimeInState() > 0.2f)) ||
		((GetState() > State_Pickup) && (GetState() <= State_Putdown));
	if(bMustDraggedTaskBeActive)
	{
		//Make sure the dragged ped has the correct task.
		if(!pDragged->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_DRAGGED_TO_SAFETY, true))
		{
			return FSM_Quit;
		}
	}
	
	//Check if the abort flag has been set.
	if(m_uRunningFlags.IsFlagSet(RF_ShouldQuit))
	{
		return FSM_Quit;
	}
	
	//Check if the target is too close.
	if(IsTargetTooClose())
	{
		return FSM_Quit;
	}
	
	//Process the timers.
	ProcessTimers();
	
	//Process the cover point.
	ProcessCoverPoint();
	
	//Check if we need a position to drag to.
	if(NeedsPositionToDragTo())
	{
		//Ensure we can generate a position to drag to.
		if(!CanGeneratePositionToDragTo())
		{
			return FSM_Quit;
		}
	}
	
	//Process the ragdolls.
	if(!ProcessRagdolls())
	{
		return FSM_Quit;
	}
	
	//Process streaming.
	ProcessStreaming();
	
	//Don't allow low-lod mode for the motion task.
	//Allowing the ped to go into low-lod mode will not allow them to
	//strafe backwards, and they will never reach their drag destination.
	pPed->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodMotionTask);

	//Disable time slicing.
	pPed->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate);

	// While we are running this task, keep our ragdoll matrix updated every frame.
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ForceUpdateRagdollMatrix, true);

	//Process the look at.
	ProcessLookAt();

	//Process the probes.
	ProcessProbes();

	//Process the obstructions.
	if(!ProcessObstructions())
	{
		return FSM_Quit;
	}
	
	return FSM_Continue;
}

CTask::FSM_Return CTaskDraggingToSafety::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin
	
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();
				
		FSM_State(State_Stream)
			FSM_OnEnter
				Stream_OnEnter();
			FSM_OnUpdate
				return Stream_OnUpdate();
				
		FSM_State(State_CallForCover)
			FSM_OnEnter
				CallForCover_OnEnter();
			FSM_OnUpdate
				return CallForCover_OnUpdate();
				
		FSM_State(State_Approach)
			FSM_OnEnter
				Approach_OnEnter();
			FSM_OnUpdate
				return Approach_OnUpdate();
				
		FSM_State(State_PickupEntry)
			FSM_OnEnter
				PickupEntry_OnEnter();
			FSM_OnUpdate
				return PickupEntry_OnUpdate();
			FSM_OnExit
				PickupEntry_OnExit();
				
		FSM_State(State_Pickup)
			FSM_OnEnter
				Pickup_OnEnter();
			FSM_OnUpdate
				return Pickup_OnUpdate();
			FSM_OnExit
				Pickup_OnExit();
				
		FSM_State(State_Dragging)
			FSM_OnEnter
				Dragging_OnEnter();
			FSM_OnUpdate
				return Dragging_OnUpdate();
				
		FSM_State(State_Adjust)
			FSM_OnEnter
				Adjust_OnEnter();
			FSM_OnUpdate
				return Adjust_OnUpdate();
				
		FSM_State(State_Putdown)
			FSM_OnEnter
				Putdown_OnEnter();
			FSM_OnUpdate
				return Putdown_OnUpdate();
			FSM_OnExit
				Putdown_OnExit();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
			
	FSM_End
}

CTask::FSM_Return CTaskDraggingToSafety::ProcessPostFSM()
{	
	//Check if the ped should face backwards.
 	if(ShouldFaceBackwards())
 	{
 		//Note: Can't use nav mesh waypoint information here because the point the ped actually moves to
 		//		is slightly different than the waypoint, which causes a bunch of heading issues.  It is also
 		//		possible for the nav mesh to delegate additional movement tasks (such as walking around entities)
 		//		that the nav mesh waypoints will not be aware of.
 		
 		//Grab the ped.
 		CPed* pPed = GetPed();
 		
 		//Keep track of the target.
 		Vector3 vTarget;
 		bool bTargetValid = false;
 		
 		//Find the go to point task.
 		CTask* pTask = pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_GO_TO_POINT);
 		if(pTask)
 		{
 			//Set the target.
 			vTarget = static_cast<CTaskMoveGoToPoint *>(pTask)->GetTarget();
 			bTargetValid = true;
 		}
 		
 		//Check if the target is valid.
 		if(!bTargetValid)
 		{
 			//Check for a go to point on route task.
			pTask = pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_GO_TO_POINT_ON_ROUTE);
			if(pTask)
			{
				//Set the target.
				vTarget = static_cast<CTaskMoveGoToPointOnRoute *>(pTask)->GetTarget();
				bTargetValid = true;
			}
 		}
 		
 		//Check if the target is valid.
 		if(bTargetValid)
 		{
			//Face the opposite direction.
			Vector3 vPos(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()));
			Vector3 vDiff = vTarget - vPos;
			pPed->SetDesiredHeading(vPos - vDiff);
 		}
 	}
 		
	return FSM_Continue;
}

bool CTaskDraggingToSafety::ProcessPhysics(float fTimeStep, int nTimeSlice)
{
	if(GetDragged())
	{
		//Check the state.
		if(GetState() == State_PickupEntry)
		{
			//Check if the clip is valid.
			const crClip* pClip = GetClipHelper() ? GetClipHelper()->GetClip() : NULL;
			if(pClip)
			{
				//Calculate the approach target and heading.
				Vec3V vTargetPosition;
				float fTargetHeading;
				CalculateApproachTargetAndHeading(RC_VECTOR3(vTargetPosition), fTargetHeading);

				//Adjust the position.
				static dev_float s_fMaxPositionChangePerSecond = 1.0f;
				CPositionFixupHelper::Apply(*GetPed(), vTargetPosition, *pClip, GetClipHelper()->GetPhase(),
					s_fMaxPositionChangePerSecond, fTimeStep, CPositionFixupHelper::F_IgnoreZ);

				//Adjust the heading.
				static dev_float s_fMaxHeadingChangePerSecond = 1.5f;
				CHeadingFixupHelper::Apply(*GetPed(), fTargetHeading, *pClip, GetClipHelper()->GetPhase(),
					s_fMaxHeadingChangePerSecond, fTimeStep);
			}
		}
	}

	return CTask::ProcessPhysics(fTimeStep, nTimeSlice);
}

CTask::FSM_Return CTaskDraggingToSafety::Start_OnUpdate()
{
	//Check if someone else is already dragging the ped.
	//This can happen if two peds try at the same time.
	if(GetDragged()->GetPedConfigFlag(CPED_CONFIG_FLAG_IsBeingDraggedToSafety))
	{
		//Finish the task.
		SetState(State_Finish);
	}
	else
	{
		//Note that the ped is being dragged to safety.
		GetDragged()->SetPedConfigFlag(CPED_CONFIG_FLAG_IsBeingDraggedToSafety, true);
		
		//Set the drag flag.
		m_uRunningFlags.SetFlag(RF_IsDraggingToSafety);
		
		//Move to the stream state.
		SetState(State_Stream);
	}
	
	return FSM_Continue;
}

void CTaskDraggingToSafety::Stream_OnEnter()
{
	//Create the task.
	CTask* pTask = CreateCombatTask();
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskDraggingToSafety::Stream_OnUpdate()
{
	//Check if the clip sets are loaded.
	if(m_ClipSetRequestHelper.IsLoaded() && m_ClipSetRequestHelperForPickupEntry.IsLoaded())
	{
		//Call for cover.
		SetState(State_CallForCover);
	}
	//Check if the time has expired.
	else if(GetTimeInState() > sm_Tunables.m_MaxTimeForStream)
	{
		//Finish the task.
		SetState(State_Finish);
	}
	
	return FSM_Continue;
}

void CTaskDraggingToSafety::CallForCover_OnEnter()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	//Ask for cover.
	pPed->NewSay("COVER_ME");
	
	//Call for cover.
	CEventCallForCover event(pPed);
	event.CommunicateEvent(pPed, true, false);
	
	//Create the task.
	CTask* pTask = CreateCombatTask();
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskDraggingToSafety::CallForCover_OnUpdate()
{
	//Check if cover has been provided.
	if(m_uRunningFlags.IsFlagSet(RF_HasCoverBeenProvided) || m_uConfigFlags.IsFlagSet(CF_CoverNotRequired))
	{
		//Approach the injured ped.
		SetState(State_Approach);
	}
	else if(GetTimeInState() > sm_Tunables.m_CoverResponseTimeout)
	{
		//Finish the task.
		SetState(State_Finish);
	}
	
	return FSM_Continue;
}

void CTaskDraggingToSafety::Approach_OnEnter()
{
	//Grab the ped.
	CPed* pPed = GetPed();
	
	//Ensure the ped is standing.
	pPed->SetIsCrouching(false);
	
	//Say the audio.
	pPed->NewSay("RESCUE_INJURED_COP");
	
	//Create the move task.
	CTask* pMoveTask = CreateMoveTaskForApproach();
	
	//Create the task.
	CTask* pTask = rage_new CTaskComplexControlMovement(pMoveTask);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskDraggingToSafety::Approach_OnUpdate()
{
	//Update the move task for approach.
	UpdateMoveTaskForApproach();
	
	//Update the holster for approach.
	UpdateHolsterForApproach();
	
	//Check if the ped is unable to find a route to the target.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || NavMeshIsUnableToFindRoute())
	{
		//Finish the task.
		SetState(State_Finish);
	}
	//Check if we aren't fire resistant, and the dragged ped is on fire.
	else if(!GetPed()->IsFireResistant() && g_fireMan.IsEntityOnFire(GetDragged()))
	{
		//Finish the task.
		SetState(State_Finish);
	}
	//Check if we are close enough for an entry.
	else if(IsCloseEnoughForPickupEntry())
	{
		//Move to the entry.
		SetState(State_PickupEntry);
	}
	
	return FSM_Continue;
}

void CTaskDraggingToSafety::PickupEntry_OnEnter()
{
	//Do not collide with the dragged ped.
	GetPed()->SetNoCollision(GetDragged(), NO_COLLISION_PERMENANT);

	//Choose a clip for entry.
	fwMvClipId clipId = ChooseClipForPickupEntry();
	taskAssert(clipId != CLIP_ID_INVALID);

	//Start the clip.
	StartClipBySetAndClip(GetPed(), m_ClipSetRequestHelperForPickupEntry.GetClipSetId(), clipId, SLOW_BLEND_IN_DELTA);
}

CTask::FSM_Return CTaskDraggingToSafety::PickupEntry_OnUpdate()
{
	//Process physics.
	GetPed()->SetPedResetFlag(CPED_RESET_FLAG_ProcessPhysicsTasks, true);

	//Make sure the clip exists.
	if(!GetClipHelper())
	{
		//Finish the task.
		SetState(State_Finish);
	}
	else if(GetClipHelper()->IsHeldAtEnd())
	{
		//Give the ped the dragged task.
		CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY, rage_new CTaskDraggedToSafety(GetPed()), true, E_PRIORITY_DRAGGING, true);
		GetDragged()->GetPedIntelligence()->AddEvent(event);

		//Force an AI update this frame.
		GetDragged()->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAIUpdate, true);

		//Make sure time slicing allows our event through as quickly as possible.
		GetDragged()->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);

		//Pick up the ped.
		SetState(State_Pickup);
	}

	return FSM_Continue;
}

void CTaskDraggingToSafety::PickupEntry_OnExit()
{
	//Stop the clip.
	StopClip(SLOW_BLEND_OUT_DELTA);
}

void CTaskDraggingToSafety::Pickup_OnEnter()
{
	//Ensure we are within a reasonable distance.
	if(!IsWithinReasonableDistanceOfApproachTarget())
	{
		return;
	}

	//Grab the ped.
	CPed* pPed = GetPed();
	
	//Play the sound for requesting cover.
	pPed->NewSay("COVER_ME");
	
	//Call for cover.
	CEventCallForCover event(pPed);
	event.CommunicateEvent(pPed, true, false);
	
	//Stop all motion.
	pPed->StopAllMotion(true);
	
	//Choose the pickup direction.
	m_nPickupDirection = ChoosePickupDirection();
	
	//Choose the pickup clip.
	fwMvClipId clipId = ChoosePickupClip();

	//Start the pickup clip.
	StartClipBySetAndClip(pPed, m_ClipSetRequestHelper.GetClipSetId(), clipId, NORMAL_BLEND_IN_DELTA);
}

CTask::FSM_Return CTaskDraggingToSafety::Pickup_OnUpdate()
{
	//Make sure the clip exists.
	if(!GetClipHelper())
	{
		//Finish the task.
		SetState(State_Finish);
	}
	else if(GetClipHelper()->IsHeldAtEnd())
	{
		//Animation is complete, drag the ped to cover.
		SetState(State_Dragging);
	}
	
	return FSM_Continue;
}

void CTaskDraggingToSafety::Pickup_OnExit()
{
	//Stop the clip.
	StopClip(SLOW_BLEND_OUT_DELTA);
}

void CTaskDraggingToSafety::Dragging_OnEnter()
{
	//Check if we are friendly with the dragged ped.
	if(GetPed()->GetPedIntelligence()->IsFriendlyWith(*GetDragged()))
	{
		//Say the audio.
		GetPed()->NewSay("RESCUE_INJURED_BUDDY");
	}

	//Generate the position to drag to.
	Vec3V vPositionToDragTo;
	if(!GeneratePositionToDragTo(vPositionToDragTo))
	{
		vPositionToDragTo = GetPed()->GetTransform().GetPosition();
	}
	
	//Set up the nav mesh.
	CNavParams params;
	params.m_vTarget = VEC3V_TO_VECTOR3(vPositionToDragTo);
	params.m_fTargetRadius = 0.25f;
	params.m_fMoveBlendRatio = 0.5f;
	params.m_fSlowDownDistance = 2.0f;
	params.m_fEntityRadiusOverride = 0.5f;

	//Create the move task.
	CTaskMoveFollowNavMesh* pMoveTask = rage_new CTaskMoveFollowNavMesh(params);
	
	//Keep moving while waiting for a path.
	pMoveTask->SetKeepMovingWhilstWaitingForPath(true);
	
	//Quit the task if a route is not found.
	pMoveTask->SetQuitTaskIfRouteNotFound(true);
	
	//Do not use ladders, climbs, or drops.
	pMoveTask->SetDontUseLadders(true);
	pMoveTask->SetDontUseClimbs(true);
	pMoveTask->SetDontUseDrops(true);
	
	//Stop exactly.
	pMoveTask->SetStopExactly(true);

	//Drag the ped to the cover point.
	SetNewTask(rage_new CTaskComplexControlMovement(pMoveTask, rage_new CTaskRunClip(m_ClipSetRequestHelper.GetClipSetId(), CLIP_INJURED_DRAG_PLYR, INSTANT_BLEND_IN_DELTA, NORMAL_BLEND_OUT_DELTA, -1, CTaskClip::ATF_SECONDARY), CTaskComplexControlMovement::TerminateOnBoth));
}

CTask::FSM_Return CTaskDraggingToSafety::Dragging_OnUpdate()
{
	//Set the strafing flag.
	GetPed()->GetMotionData()->SetIsStrafing(true);
	
	//Check if a route could not be found.
	if(NavMeshIsUnableToFindRoute())
	{
		//Finish the task.
		SetState(State_Finish);
	}
	//Check if we are within a reasonable distance.
	else if(!ArePedsWithinDistance(*GetPed(), *GetDragged(), 3.5f))
	{
		SetState(State_Finish);
	}
	//Check if the sub-task has finished.
	else if(FindSubTaskOfType(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT) == NULL || 
		(GetPed()->GetPedIntelligence()->GetGeneralMovementTask() != NULL && GetPed()->GetPedIntelligence()->GetGeneralMovementTask()->GetIsFlagSet(aiTaskFlags::HasFinished)))
	{
		//Note that we have reached the position to drag to.
		m_uRunningFlags.SetFlag(RF_HasReachedPositionToDragTo);

		//Put the ped down.
		SetState(State_Putdown);
	}
	
	return FSM_Continue;
}

void CTaskDraggingToSafety::Adjust_OnEnter()
{
	//Generate the position to drag to.
	Vec3V vPositionToDragTo;
	if(!GeneratePositionToDragTo(vPositionToDragTo))
	{
		vPositionToDragTo = GetPed()->GetTransform().GetPosition();
	}
	
	//Build up the nav params.
	CNavParams params;
	params.m_vTarget = VEC3V_TO_VECTOR3(vPositionToDragTo);
	params.m_fMoveBlendRatio = 0.5f;
	params.m_bFleeFromTarget = true;
	params.m_fSlowDownDistance = 0.0f;
	params.m_fFleeSafeDistance = Abs(sm_Tunables.m_SeparationPutdown.y);

	//Create the move task.
	CTaskMoveFollowNavMesh* pMoveTask = rage_new CTaskMoveFollowNavMesh(params);

	//Quit the task if a route is not found.
	pMoveTask->SetQuitTaskIfRouteNotFound(true);

	//Drag the ped to the cover point.
	CTaskComplexControlMovement* pSubTask = static_cast<CTaskComplexControlMovement*>(FindSubTaskOfType(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT));
	if(pSubTask == NULL)
	{
		SetNewTask(rage_new CTaskComplexControlMovement(pMoveTask, rage_new CTaskRunClip(m_ClipSetRequestHelper.GetClipSetId(), CLIP_INJURED_DRAG_PLYR, INSTANT_BLEND_IN_DELTA, INSTANT_BLEND_OUT_DELTA, -1, CTaskClip::ATF_SECONDARY)));
	}
	else
	{
		pSubTask->SetNewMoveTask(pMoveTask);
		pSubTask->SetTerminationType(CTaskComplexControlMovement::TerminateOnMovementTask);
	}
}

CTask::FSM_Return CTaskDraggingToSafety::Adjust_OnUpdate()
{
	//Grab the ped.
	CPed* pPed = GetPed();
	
	//Set the strafing flag.
	pPed->GetMotionData()->SetIsStrafing(true);
	
	//Grab the ped position.
	Vec3V vPedPosition = pPed->GetTransform().GetPosition();
	
	//Generate the position to drag to.
	Vec3V vPositionToDragTo;
	if(!GeneratePositionToDragTo(vPositionToDragTo))
	{
		vPositionToDragTo = vPedPosition;
	}
	
	//Check the distance from the target.
	ScalarV scDistSq = DistSquared(vPedPosition, vPositionToDragTo);
	
	//Check if the ped has moved away from the target.
	bool bAdjustComplete = false;
	ScalarV scMaxDistSq = ScalarVFromF32(square(sm_Tunables.m_SeparationPutdown.y));
	if(IsGreaterThanAll(scDistSq, scMaxDistSq))
	{
		//Mark the adjustment as complete.
		bAdjustComplete = true;
	}

	//Wait until the pathfinding is complete.
	if(bAdjustComplete || !GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished) || NavMeshIsUnableToFindRoute())
	{
		//Note that we have reached the position to drag to.
		m_uRunningFlags.SetFlag(RF_HasReachedPositionToDragTo);
		
		//Put the ped down.
		SetState(State_Putdown);
	}

	return FSM_Continue;
}

void CTaskDraggingToSafety::Putdown_OnEnter()
{
	//Stop all motion.
	GetPed()->StopAllMotion(true);

	//Note that the ped has been dragged to safety.
	GetDragged()->SetPedConfigFlag(CPED_CONFIG_FLAG_HasBeenDraggedToSafety, true);
	
	//Play the putdown clip.
	StartClipBySetAndClip(GetPed(), m_ClipSetRequestHelper.GetClipSetId(), CLIP_INJURED_PUTDOWN_PLYR, NORMAL_BLEND_IN_DELTA);
}

CTask::FSM_Return CTaskDraggingToSafety::Putdown_OnUpdate()
{
	//Check if the clip is invalid.
	CClipHelper* pClipHelper = GetClipHelper();
	if(!pClipHelper)
	{
		//Couldn't play the clip, just quit.
		SetState(State_Finish);
	}
	//Check if we should interrupt.
	else if(m_uConfigFlags.IsFlagSet(CF_Interrupt) && pClipHelper->IsEvent(CClipEventTags::Interruptible))
	{
		//Blend the clip out slow.
		pClipHelper->StopClip(SLOW_BLEND_OUT_DELTA);

		//Finish the task.
		SetState(State_Finish);
	}
	//Check if the clip has finished.
	else if(pClipHelper->IsHeldAtEnd())
	{
		//Finish the task.
		SetState(State_Finish);
	}

	return FSM_Continue;
}

void CTaskDraggingToSafety::Putdown_OnExit()
{
	//Stop the clip.
	StopClip(SLOW_BLEND_OUT_DELTA);
}

void CTaskDraggingToSafety::OnGunAimedAt(CPed* pAimingPed)
{
	//Ensure the aiming ped is valid.
	if(!pAimingPed)
	{
		return;
	}
	
	//Ensure the ped is valid.
	CPed* pPed = GetPed();
	if(!pPed)
	{
		return;
	}
	
	//Check if the entity is within a certain distance.
	ScalarV scMinDistSq = ScalarVFromF32(rage::square(sm_Tunables.m_AbortAimedAtMinDistance));
	ScalarV scDistSq = DistSquared(pPed->GetTransform().GetPosition(), pAimingPed->GetTransform().GetPosition());
	if(!IsLessThanAll(scDistSq, scMinDistSq))
	{
		return;
	}
	
	//Abort the task.
	m_uRunningFlags.SetFlag(RF_ShouldQuit);
}

void CTaskDraggingToSafety::OnProvidingCover(const CEventProvidingCover& UNUSED_PARAM(rEvent))
{
	//Set the cover provided flag.
	m_uRunningFlags.SetFlag(RF_HasCoverBeenProvided);
}

bool CTaskDraggingToSafety::ConvertPedToHighLodRagdoll(CPed* pPed)
{
	//Ensure the ped is valid.
	if(!pPed)
	{
		return false;
	}

	//Ensure the ragdoll inst is valid.
	fragInstNMGta* pInst = pPed->GetRagdollInst();
	if(!pInst)
	{
		return false;
	}

	//Set the LOD to high, if necessary.
	if(pInst->GetCurrentPhysicsLOD() != fragInst::RAGDOLL_LOD_HIGH)
	{
		// Ragdoll LOD switching is currently only supported on inactive insts.  Should we switch to animated here?
		if (PHLEVEL->IsInactive(pPed->GetRagdollInst()->GetLevelIndex()))
		{
			pInst->SetCurrentPhysicsLOD(fragInst::RAGDOLL_LOD_HIGH);
		}
	}

	return (pInst->GetCurrentPhysicsLOD() == fragInst::RAGDOLL_LOD_HIGH);
}

bool CTaskDraggingToSafety::ArePedsWithinDistance(const CPed& rPed, const CPed& rOther, float fMaxDistance) const
{
	//Calculate the distance.
	float fDistanceSq = DistSquared(rPed.GetTransform().GetPosition(), rOther.GetTransform().GetPosition()).Getf();
	float fMaxDistanceSq = square(fMaxDistance);

	return (fDistanceSq < fMaxDistanceSq);
}

void CTaskDraggingToSafety::CalculateApproachTarget(Vector3& vTargetPosition) const
{
	//Calculate the approach target and heading.
	float fTargetHeading;
	CalculateApproachTargetAndHeading(vTargetPosition, fTargetHeading);
}

void CTaskDraggingToSafety::CalculateApproachTargetAndHeading(Vector3& vTargetPosition, float& fTargetHeading) const
{
	//Calculate the target at the pickup distance.
	CalculateTargetAndHeading(sm_Tunables.m_SeparationPickup, vTargetPosition, fTargetHeading);
}

void CTaskDraggingToSafety::CalculateTargetAndHeading(const Vector3& vOffset, Vector3& vTargetPosition, float& fTargetHeading) const
{
	//Grab the dragged ped.
	const CPed* pDragged = GetDragged();
	
	if(pDragged)
	{
		//Get the dragged root.
		Matrix34 mDraggedRoot;
		pDragged->GetGlobalMtx(pDragged->GetBoneIndexFromBoneTag(BONETAG_ROOT), mDraggedRoot);
		
		//Get the dragged head.
		Matrix34 mDraggedHead;
		pDragged->GetGlobalMtx(pDragged->GetBoneIndexFromBoneTag(BONETAG_HEAD), mDraggedHead);

		//Get the dragged pelvis.
		Matrix34 mDraggedPelvis;
		pDragged->GetGlobalMtx(pDragged->GetBoneIndexFromBoneTag(BONETAG_PELVIS), mDraggedPelvis);
		
		//Calculate the forward vector.
		Vector3 vForward = mDraggedPelvis.d - mDraggedHead.d;
		vForward.z = 0.0f;
		vForward.NormalizeSafe();
		
		//Calculate the up vector.
		Vector3 vUp = ZAXIS;

		//Calculate the side vector.
		Vector3 vSide;
		vSide.Cross(vForward, vUp);

		//Generate the transform.
		Matrix34 mTransform;
		mTransform.a = vSide;
		mTransform.b = vForward;
		mTransform.c = vUp;
		mTransform.d = mDraggedRoot.d;
		
		//Calculate the target position.
		mTransform.Transform(vOffset, vTargetPosition);

		//Calculate the target heading.
		Vector3 vYAxis = VEC3_ZERO; 
		vYAxis.y = 1.0f;
		Vector3 vDest = mDraggedRoot.d - vTargetPosition;
		vDest.z = 0.0f;
		vDest.NormalizeSafe(); 
		fTargetHeading = fwAngle::LimitRadianAngle(vYAxis.AngleZ(vDest));
	}
	else
	{
		// In the case of no dragged ped, init to current position
		const CPed* pPed = GetPed();
		vTargetPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		fTargetHeading = pPed->GetCurrentHeading();
	}
}

bool CTaskDraggingToSafety::CanGeneratePositionToDragTo() const
{
	//Check if we have a position to drag to.
	if(m_uRunningFlags.IsFlagSet(RF_HasPositionToDragTo))
	{
		return true;
	}
	
	//Check if we have a cover point to drag to.
	if(HasCoverPoint())
	{
		return true;
	}
	
	return false;
}

fwMvClipId CTaskDraggingToSafety::ChooseClipForPickupEntry() const
{
	//Define the clips.
	static fwMvClipId s_Clip0Id("injured_pickup_0_plyr",0x5DD950D1);
	static fwMvClipId s_Clip90Id("injured_pickup_90_plyr",0x99B20671);
	static fwMvClipId s_ClipNeg90Id("injured_pickup_-90_plyr",0x70CDC680);
	static fwMvClipId s_Clip180Id("injured_pickup_180_plyr",0x885BA093);
	static fwMvClipId s_ClipNeg180Id("injured_pickup_-180_plyr",0x554240C);

	//Assert that the clip set is streamed in.
	taskAssert(m_ClipSetRequestHelperForPickupEntry.IsLoaded());

	//Get the mover track translations.
	static Vec3V s_vTranslation0 = VECTOR3_TO_VEC3V(fwAnimHelpers::GetMoverTrackTranslationDiff(
		m_ClipSetRequestHelperForPickupEntry.GetClipSetId(), s_Clip0Id, 0.0f, 1.0f));
	static Vec3V s_vTranslation90 = VECTOR3_TO_VEC3V(fwAnimHelpers::GetMoverTrackTranslationDiff(
		m_ClipSetRequestHelperForPickupEntry.GetClipSetId(), s_Clip90Id, 0.0f, 1.0f));
	static Vec3V s_vTranslationNeg90 = VECTOR3_TO_VEC3V(fwAnimHelpers::GetMoverTrackTranslationDiff(
		m_ClipSetRequestHelperForPickupEntry.GetClipSetId(), s_ClipNeg90Id, 0.0f, 1.0f));
	static Vec3V s_vTranslation180 = VECTOR3_TO_VEC3V(fwAnimHelpers::GetMoverTrackTranslationDiff(
		m_ClipSetRequestHelperForPickupEntry.GetClipSetId(), s_Clip180Id, 0.0f, 1.0f));
	static Vec3V s_vTranslationNeg180 = VECTOR3_TO_VEC3V(fwAnimHelpers::GetMoverTrackTranslationDiff(
		m_ClipSetRequestHelperForPickupEntry.GetClipSetId(), s_ClipNeg180Id, 0.0f, 1.0f));

	//Calculate the approach target and heading.
	Vec3V vTargetPosition;
	float fTargetHeading;
	CalculateApproachTargetAndHeading(RC_VECTOR3(vTargetPosition), fTargetHeading);

	//Create the target matrix.
	Matrix34 mTarget;
	mTarget.MakeRotateZ(fwAngle::LimitRadianAngleSafe(fTargetHeading));
	mTarget.d = VEC3V_TO_VECTOR3(vTargetPosition);

	//Calculate the positions.
	Vec3V vPosition0		= Transform(MATRIX34_TO_MAT34V(mTarget), s_vTranslation0);
	Vec3V vPosition90		= Transform(MATRIX34_TO_MAT34V(mTarget), s_vTranslation90);
	Vec3V vPositionNeg90	= Transform(MATRIX34_TO_MAT34V(mTarget), s_vTranslationNeg90);
	Vec3V vPosition180		= Transform(MATRIX34_TO_MAT34V(mTarget), s_vTranslation180);
	Vec3V vPositionNeg180	= Transform(MATRIX34_TO_MAT34V(mTarget), s_vTranslationNeg180);

	//Calculate the distances.
	ScalarV scDistSq0		= DistSquared(GetPed()->GetTransform().GetPosition(), vPosition0);
	ScalarV scDistSq90		= DistSquared(GetPed()->GetTransform().GetPosition(), vPosition90);
	ScalarV scDistSqNeg90	= DistSquared(GetPed()->GetTransform().GetPosition(), vPositionNeg90);
	ScalarV scDistSq180		= DistSquared(GetPed()->GetTransform().GetPosition(), vPosition180);
	ScalarV scDistSqNeg180	= DistSquared(GetPed()->GetTransform().GetPosition(), vPositionNeg180);

	//Check which clip is the closest.
	if(IsLessThanAll(scDistSq0, scDistSq90) && IsLessThanAll(scDistSq0, scDistSqNeg90) &&
		IsLessThanAll(scDistSq0, scDistSq180) && IsLessThanAll(scDistSq0, scDistSqNeg180))
	{
		return s_Clip0Id;
	}
	else if(IsLessThanAll(scDistSq90, scDistSqNeg90) && IsLessThanAll(scDistSq90, scDistSq180) &&
		IsLessThanAll(scDistSq90, scDistSqNeg180))
	{
		return s_Clip90Id;
	}
	else if(IsLessThanAll(scDistSqNeg90, scDistSq180) && IsLessThanAll(scDistSqNeg90, scDistSqNeg180))
	{
		return s_ClipNeg90Id;
	}
	else if(IsLessThanAll(scDistSq180, scDistSqNeg180))
	{
		return s_Clip180Id;
	}
	else
	{
		return s_ClipNeg180Id;
	}
}

fwMvClipId CTaskDraggingToSafety::ChoosePickupClip() const
{
	//Check the pickup direction.
	switch(m_nPickupDirection)
	{
		case PD_Front:
		{
			return CLIP_INJURED_PICKUP_FRONT_PLYR;
		}
		case PD_Back:
		{
			return CLIP_INJURED_PICKUP_BACK_PLYR;
		}
		case PD_SideLeft:
		{
			return CLIP_INJURED_PICKUP_SIDE_LEFT_PLYR;
		}
		case PD_SideRight:
		{
			return CLIP_INJURED_PICKUP_SIDE_RIGHT_PLYR;
		}
		default:
		{
			taskAssertf(false, "Unknown pickup direction: %d.", m_nPickupDirection);
			return CLIP_ID_INVALID;
		}
	}
}

CTaskDraggingToSafety::PickupDirection CTaskDraggingToSafety::ChoosePickupDirection() const
{
	//Get the dragged ped.
	const CPed* pDragged = GetDragged();
	
	//Grab the left armroll position.
	Vector3 vLeftArmRollPosition;
	pDragged->GetBonePosition(vLeftArmRollPosition, BONETAG_L_ARMROLL);
	
	//Grab the right armroll position.
	Vector3 vRightArmRollPosition;
	pDragged->GetBonePosition(vRightArmRollPosition, BONETAG_R_ARMROLL);
	
	//Generate the direction from the left armroll to the right armroll.
	Vector3 vLeftArmRollToRightArmRoll = (vRightArmRollPosition - vLeftArmRollPosition);
	Vector3 vLeftArmRollToRightArmRollDirection = vLeftArmRollToRightArmRoll;
	vLeftArmRollToRightArmRollDirection.NormalizeFast();
	
	//Check if the ped is on their side.
	float fDot = vLeftArmRollToRightArmRollDirection.Dot(ZAXIS);
	if(fDot > sm_Tunables.m_MinDotForPickupDirection)
	{
		return PD_SideLeft;
	}
	else if(Abs(fDot) > sm_Tunables.m_MinDotForPickupDirection)
	{
		return PD_SideRight;
	}
	
	//Grab the spine matrix.
	Matrix34 mSpine;
	pDragged->GetBoneMatrix(mSpine, BONETAG_SPINE3);
	
	//Grab the spine direction.
	Vector3 vSpineDirection = mSpine.b;
	
	//Check if the ped is on their back/front.
	fDot = vSpineDirection.Dot(ZAXIS);
	if(fDot > sm_Tunables.m_MinDotForPickupDirection)
	{
		return PD_Back;
	}
	else if(Abs(fDot) > sm_Tunables.m_MinDotForPickupDirection)
	{
		return PD_Front;
	}
	
	//At this point the ped didn't really match any of the poses.
	//I guess we can use the back by default.
	//Maybe we should just not pick them up...
	return PD_Back;
}

void CTaskDraggingToSafety::ClearCoverPoint()
{
	//Clear the cover point.
	SetCoverPoint(NULL);
}

CTask* CTaskDraggingToSafety::CreateCombatTask() const
{
	//Ensure the primary target is valid.
	if(!m_pPrimaryTarget)
	{
		return NULL;
	}
	
	return rage_new CTaskCombatAdditionalTask(CTaskCombatAdditionalTask::RT_Default, m_pPrimaryTarget, VEC3V_TO_VECTOR3(m_pPrimaryTarget->GetTransform().GetPosition()));
}

CTask* CTaskDraggingToSafety::CreateMoveTaskForApproach() const
{
	//Calculate the approach target.
	Vector3 vTargetPosition;
	CalculateApproachTarget(vTargetPosition);

	//Set up the nav mesh.
	CNavParams params;
	params.m_vTarget = vTargetPosition;
	params.m_fMoveBlendRatio = m_fMoveBlendRatioForApproach;

	//Create the task.
	CTaskMoveFollowNavMesh* pTask = rage_new CTaskMoveFollowNavMesh(params);

	//Do not use ladders, climbs, or drops.
	pTask->SetDontUseLadders(true);
	pTask->SetDontUseClimbs(true);
	pTask->SetDontUseDrops(true);

	//Stop exactly.
	pTask->SetStopExactly(true);
	
	return pTask;
}

bool CTaskDraggingToSafety::DoWeCareAboutObstructions() const
{
	//Ensure the state is is valid.
	if(GetState() > State_Approach)
	{
		return false;
	}

	return true;
}

void CTaskDraggingToSafety::FindCoverPoint()
{
	//Clear the time since the last cover point search.
	m_fTimeSinceLastCoverPointSearch = 0.0f;
	
	//Create the dragging cover point data.
	DraggingCoverPointData data;
	data.m_vPosition = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
	data.m_pPed = GetPed();
	data.m_pDragged = GetDragged();
	
	//Check if the target is valid.
	if(m_pPrimaryTarget.Get())
	{
		//Set the target position.
		data.m_vTargetPosition = VEC3V_TO_VECTOR3(m_pPrimaryTarget->GetTransform().GetPosition());
		
		//Note that the target position should be used.
		data.m_bUseTargetPosition = true;
	}
	else
	{
		//Do not use the target position.
		data.m_bUseTargetPosition = false;
	}

	//Find the closest cover point.
	CCoverPoint* pCoverPoint = CCover::FindClosestCoverPointWithCB(data.m_pPed, data.m_vPosition, sm_Tunables.m_CoverMaxDistance, data.m_bUseTargetPosition ? &data.m_vTargetPosition : NULL, CCover::CS_MUST_PROVIDE_COVER, CTaskDraggingToSafety::CoverPointScoringCB, CTaskDraggingToSafety::CoverPointValidityCB, &data);
	
	//Ensure the cover point is valid.
	if(!pCoverPoint)
	{
		return;
	}
	
	//Set the cover point.
	SetCoverPoint(pCoverPoint);
}

bool CTaskDraggingToSafety::GeneratePositionToDragTo(Vec3V_InOut vPositionToDragTo) const
{
	//Check if we have a position to drag to.
	if(m_uRunningFlags.IsFlagSet(RF_HasPositionToDragTo))
	{
		vPositionToDragTo = m_vPositionToDragTo;
		return true;
	}
	//Check if the cover point is valid.
	else if(m_pCoverPoint)
	{
		m_pCoverPoint->GetCoverPointPosition(RC_VECTOR3(vPositionToDragTo));
		return true;
	}
	else
	{
		return false;
	}
}

CTaskMoveFollowNavMesh* CTaskDraggingToSafety::GetSubTaskNavMesh()
{
	//Ensure a sub-task exists.
	if(!GetSubTask())
	{
		return NULL;
	}

	//Ensure the task is a complex control movement.
	if(GetSubTask()->GetTaskType() != CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
	{
		return NULL;
	}

	//Grab the running movement task.
	aiTask* pTask = static_cast<CTaskComplexControlMovement *>(GetSubTask())->GetRunningMovementTask(GetPed());
	if(!pTask)
	{
		return NULL;
	}

	//Ensure the task is a nav mesh task.
	if(pTask->GetTaskType() != CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
	{
		return NULL;
	}

	return static_cast<CTaskMoveFollowNavMesh *>(pTask);
}

bool CTaskDraggingToSafety::HasCoverPoint() const
{
	return (m_pCoverPoint != NULL);
}

bool CTaskDraggingToSafety::IsCloseEnoughForPickupEntry() const
{
	//Choose a clip for entry.
	fwMvClipId clipId = ChooseClipForPickupEntry();
	taskAssert(clipId != CLIP_ID_INVALID);

	//Ensure the clip is valid.
	const crClip* pClip = fwAnimManager::GetClipIfExistsBySetId(m_ClipSetRequestHelperForPickupEntry.GetClipSetId(), clipId);
	if(!pClip)
	{
		return false;
	}

	//Get the mover track translation.
	Vec3V vTranslation = VECTOR3_TO_VEC3V(fwAnimHelpers::GetMoverTrackTranslationDiff(*pClip, 0.0f, 1.0f));

	//Grab the approach target.
	Vec3V vApproachTarget;
	CalculateApproachTarget(RC_VECTOR3(vApproachTarget));

	//Check if we are within range.
	ScalarV scDistSq = DistSquared(GetPed()->GetTransform().GetPosition(), vApproachTarget);
	ScalarV scMaxDistSq = MagSquared(vTranslation);
	if(IsGreaterThanAll(scDistSq, scMaxDistSq))
	{
		return false;
	}

	return true;
}

bool CTaskDraggingToSafety::IsCoverPointInvalid() const
{
	//Ensure the cover point is valid.
	if(!m_pCoverPoint)
	{
		return true;
	}
	
	//Check if the cover point is invalid.
	if(m_pCoverPoint->GetType() == CCoverPoint::COVTYPE_NONE)
	{
		return true;
	}
	
	//Check if the cover point is dangerous.
	if(m_pCoverPoint->IsDangerous())
	{
		return true;
	}
	
	return false;
}

bool CTaskDraggingToSafety::IsTargetTooClose() const
{
	//Ensure the target is valid.
	if(!m_pPrimaryTarget)
	{
		return false;
	}

	//Ensure the flag is not set.
	if(m_uConfigFlags.IsFlagSet(CF_DisableTargetTooClose))
	{
		return false;
	}
	
	//Grab the max distance.
	float fMaxDistance = sm_Tunables.m_MaxDistanceToConsiderTooClose;
	
	//Check if the target is close to us.
	if(ArePedsWithinDistance(*m_pPrimaryTarget, *GetPed(), fMaxDistance))
	{
		return true;
	}
	
	//Check if the target is close to the dragged ped.
	if(ArePedsWithinDistance(*m_pPrimaryTarget, *GetDragged(), fMaxDistance))
	{
		return true;
	}
	
	return false;
}

bool CTaskDraggingToSafety::IsWithinReasonableDistanceOfApproachTarget() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Grab the ped position.
	Vec3V vPedPosition = pPed->GetTransform().GetPosition();

	//Calculate the approach target.
	Vec3V vTargetPosition;
	CalculateApproachTarget(RC_VECTOR3(vTargetPosition));

	//Account for the ground offset.
	vTargetPosition.SetZf(vTargetPosition.GetZf() + pPed->GetCapsuleInfo()->GetGroundToRootOffset());

	//Ensure the height difference is within the threshold.
	ScalarV scHeightDifference = Abs(Subtract(vPedPosition.GetZ(), vTargetPosition.GetZ()));
	ScalarV scMaxHeightDifference = ScalarVFromF32(sm_Tunables.m_MaxHeightDifferenceToApproachTarget);
	if(IsGreaterThanAll(scHeightDifference, scMaxHeightDifference))
	{
		return false;
	}

	//Flatten the positions.
	Vec3V vTargetPositionXY = vTargetPosition;
	vTargetPositionXY.SetZ(ScalarV(V_ZERO));
	Vec3V vPedPositionXY = vPedPosition;
	vPedPositionXY.SetZ(ScalarV(V_ZERO));

	//Ensure the distance is within the threshold.
	ScalarV scDistSq = DistSquared(vPedPositionXY, vTargetPositionXY);
	ScalarV scMaxDistSq = ScalarVFromF32(square(sm_Tunables.m_MaxXYDistanceToApproachTarget));
	if(IsGreaterThanAll(scDistSq, scMaxDistSq))
	{
		return false;
	}

	return true;
}

bool CTaskDraggingToSafety::NavMeshIsUnableToFindRoute()
{
	//Get the nav mesh sub-task.
	CTaskMoveFollowNavMesh* pTask = GetSubTaskNavMesh();
	if(!pTask)
	{
		return false;
	}

	return pTask->IsUnableToFindRoute();
}

bool CTaskDraggingToSafety::NeedsPositionToDragTo() const
{
	//Ensure we have not reached our position to drag to.
	if(m_uRunningFlags.IsFlagSet(RF_HasReachedPositionToDragTo))
	{
		return false;
	}
	
	return true;
}

void CTaskDraggingToSafety::ProcessCoverPoint()
{
	//Check if we have a position to drag to.
	if(m_uRunningFlags.IsFlagSet(RF_HasPositionToDragTo))
	{
		//Clear the cover point.
		ClearCoverPoint();
	}
	else
	{
		//Check if the cover point is invalid.
		if(IsCoverPointInvalid())
		{
			//Clear the cover point.
			ClearCoverPoint();
		}
		
		//Check if we should try to find a cover point.
		if(ShouldFindCoverPoint())
		{
			//Find a cover point.
			FindCoverPoint();
		}
	}
}

void CTaskDraggingToSafety::ProcessLookAt()
{
	//Update the head timer.
	m_HeadTimer.Tick();
	if(!m_HeadTimer.IsFinished())
	{
		return;
	}

	//Reset the timer.
	m_HeadTimer.Reset();

	//Check if we have a target.
	if(m_pPrimaryTarget)
	{
		//Keep track of whether to look at the target.
		bool bLookAtTarget = false;

		//Check if the target is close enough to always look at.
		float fMaxDistance = sm_Tunables.m_MaxDistanceToAlwaysLookAtTarget;
		if(ArePedsWithinDistance(*GetPed(), *m_pPrimaryTarget, fMaxDistance))
		{
			bLookAtTarget = true;
		}
		//Randomly look at the target.
		else if(fwRandom::GetRandomTrueFalse())
		{
			bLookAtTarget = true;
		}

		//Check if we should look at the target.
		if(bLookAtTarget)
		{
			//Look at the target.
			GetPed()->GetIkManager().LookAt(0, m_pPrimaryTarget, sm_Tunables.m_LookAtTime, BONETAG_HEAD, NULL, 0, 500, 500, CIkManager::IK_LOOKAT_HIGH);

			//Nothing left to do.
			return;
		}
	}

	//Check if we are dragging.
	if(GetState() == State_Dragging)
	{
		//Generate the position to drag to.
		Vector3 vPositionToDragTo;
		if(GeneratePositionToDragTo(RC_VEC3V(vPositionToDragTo)))
		{
			//Look at the drag position.
			GetPed()->GetIkManager().LookAt(0, NULL, sm_Tunables.m_LookAtTime, BONETAG_INVALID, &vPositionToDragTo, 0, 500, 500, CIkManager::IK_LOOKAT_HIGH);

			//Nothing left to do.
			return;
		}
	}
}

bool CTaskDraggingToSafety::ProcessObstructions()
{
	//Check if we care about obstructions.
	if(DoWeCareAboutObstructions())
	{
		//Check if we are obstructed.
		if(m_uRunningFlags.IsFlagSet(RF_IsObstructed))
		{
			//Increase the time we have been obstructed.
			m_fTimeWeHaveBeenObstructed += GetTimeStep();

			//Check if we have been obstructed for too long.
			float fMaxTimeToBeObstructed = sm_Tunables.m_MaxTimeToBeObstructed;
			if(m_fTimeWeHaveBeenObstructed > fMaxTimeToBeObstructed)
			{
				return false;
			}
		}
		else
		{
			//Clear the time we have been obstructed.
			m_fTimeWeHaveBeenObstructed = 0.0f;
		}
	}

	return true;
}

void CTaskDraggingToSafety::ProcessProbes()
{
	//Check if the obstruction probe is not waiting or ready.
	if(!m_ObstructionProbeResults.GetResultsWaitingOrReady() && DoWeCareAboutObstructions())
	{
		//Calculate the approach target.
		Vector3 vApproachTarget;
		CalculateApproachTarget(vApproachTarget);

		//Calculate the start/end positions.
		Vector3 vEnd = vApproachTarget;
		Vector3 vStart = vEnd;
		vEnd.z += sm_Tunables.m_ObstructionProbe.m_Height;

		//Calculate the radius.
		float fRadius = sm_Tunables.m_ObstructionProbe.m_Radius;

		//Create the probe.
		WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
		capsuleDesc.SetIsDirected(true);
		capsuleDesc.SetDoInitialSphereCheck(true);
		capsuleDesc.SetResultsStructure(&m_ObstructionProbeResults);
		capsuleDesc.SetCapsule(vStart, vEnd, fRadius);
		capsuleDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
		capsuleDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE);

		//Start the test.
		WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);

#if DEBUG_DRAW
		//Check if we should render the obstruction probe.
		if(sm_Tunables.m_Rendering.m_Enabled && sm_Tunables.m_Rendering.m_ObstructionProbe)
		{
			grcDebugDraw::Capsule(VECTOR3_TO_VEC3V(vStart), VECTOR3_TO_VEC3V(vEnd), fRadius, Color_blue);
		}
#endif
	}
	else
	{
		//Check if the obstruction probe is waiting.
		if(m_ObstructionProbeResults.GetWaitingOnResults())
		{
			//Nothing to do, we are waiting on the results.
		}
		else
		{
			//Keep track of whether we are obstructed.
			bool bIsObstructed = false;

			//Calculate the approach target.
			Vector3 vApproachTarget;
			CalculateApproachTarget(vApproachTarget);

			//Calculate the min hit position.
			//We want to ignore the ground the ped is lying on (plus a bit of fudge).
			float fMinHitPosition = vApproachTarget.z;
			fMinHitPosition += sm_Tunables.m_ObstructionProbe.m_ExtraHeightForGround;

			//Iterate over the results.
			for(int i = 0; i < m_ObstructionProbeResults.GetSize(); ++i)
			{
				//Ensure a hit was detected.
				if(!m_ObstructionProbeResults[i].GetHitDetected())
				{
					continue;
				}

				//Ignore the ground that the ped is laying on.
				float fHitPosition = m_ObstructionProbeResults[i].GetHitPosition().z;
				if(fHitPosition <= fMinHitPosition)
				{
					continue;
				}

				//Note that we are obstructed.
				bIsObstructed = true;
				break;
			}

			//Update the flag.
			m_uRunningFlags.ChangeFlag(RF_IsObstructed, bIsObstructed);

			//Reset the results.
			m_ObstructionProbeResults.Reset();
		}
	}
}

bool CTaskDraggingToSafety::ProcessRagdolls()
{
	//Ensure the dragger ped has a high-LOD ragdoll.
	if(!ConvertPedToHighLodRagdoll(GetPed()))
	{
		return false;
	}
	
	//Ensure the dragged ped has a high-LOD ragdoll.
	if(!ConvertPedToHighLodRagdoll(GetDragged()))
	{
		return false;
	}
	
	return true;
}

void CTaskDraggingToSafety::ProcessStreaming()
{
	//Request the clip set.
	static fwMvClipSetId s_ClipSetId("combat@drag_ped@",0x364F5832);
	m_ClipSetRequestHelper.Request(s_ClipSetId);

	//Request the clip set for entry.
	static fwMvClipSetId s_ClipSetIdForEntry("combat@drag_ped@entry",0xEC00861E);
	m_ClipSetRequestHelperForPickupEntry.Request(s_ClipSetIdForEntry);
}

void CTaskDraggingToSafety::ProcessTimers()
{
	//Grab the time step.
	float fTimeStep = fwTimer::GetTimeStep();
	
	//Update the timers.
	m_fTimeSinceLastCoverPointSearch += fTimeStep;
}

void CTaskDraggingToSafety::SetCoverPoint(CCoverPoint* pCoverPoint)
{
	//Ensure the cover point is changing.
	if(pCoverPoint == m_pCoverPoint)
	{
		return;
	}
	
	//Check if the cover point is valid.
	if(m_pCoverPoint)
	{
		//Release the cover point.
		m_pCoverPoint->ReleaseCoverPointForPed(GetDragged());
	}
	
	//Set the cover point.
	m_pCoverPoint = pCoverPoint;
	
	//Check if the cover point is valid.
	if(m_pCoverPoint)
	{	
		//Reserve the cover point for the dragged ped.
		m_pCoverPoint->ReserveCoverPointForPed(GetDragged());
	}
}

bool CTaskDraggingToSafety::ShouldFaceBackwards() const
{
	//Grab the state.
	s32 iState = GetState();
	
	//Only process physics in certain states.
	return ((iState == State_Dragging) || (iState == State_Adjust));
}

bool CTaskDraggingToSafety::ShouldFindCoverPoint() const
{
	//Check if the cover point is invalid.
	if(!HasCoverPoint())
	{
		return true;
	}
	
	//Check if we have not started to drag.
	//In this case, we are allowed to 'upgrade' our cover point.
	if(GetState() < State_Dragging)
	{
		//Check if the timer has expired.
		float fTimeBetweenCoverPointSearches = sm_Tunables.m_TimeBetweenCoverPointSearches;
		if(m_fTimeSinceLastCoverPointSearch >= fTimeBetweenCoverPointSearches)
		{
			return true;
		}
	}
	
	return false;
}

void CTaskDraggingToSafety::UpdateHolsterForApproach()
{
	//Ensure we have not holstered our weapon.
	if(m_uRunningFlags.IsFlagSet(RF_HasHolsteredWeapon))
	{
		return;
	}
	
	//Grab the ped.
	CPed* pPed = GetPed();
	
	//Ensure the weapon manager is valid.
	CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
	if(!pWeaponManager)
	{
		return;
	}
	
	//Ensure the ped is armed.
	if(!pWeaponManager->GetIsArmed())
	{
		return;
	}
	
	//Ensure the sub-task is valid.
	CTask* pSubTask = GetSubTask();
	if(!pSubTask)
	{
		return;
	}
	
	//Ensure the sub-task is the correct type.
	if(pSubTask->GetTaskType() != CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
	{
		return;
	}
	
	//Grab the ped position.
	Vec3V vPedPosition = pPed->GetTransform().GetPosition();
	
	//Calculate the target position.
	Vec3V vTargetPosition;
	CalculateApproachTarget(RC_VECTOR3(vTargetPosition));
	
	//Ensure the distance has exceeded the threshold.
	ScalarV scMaxDistanceForHolsterSq = ScalarVFromF32(square(sm_Tunables.m_MaxDistanceForHolster));
	ScalarV scDistSq = DistSquared(vPedPosition, vTargetPosition);
	if(IsGreaterThanAll(scDistSq, scMaxDistanceForHolsterSq))
	{
		return;
	}

	//Equip the unarmed weapon.
	pWeaponManager->EquipWeapon(pPed->GetDefaultUnarmedWeaponHash());
	
	//Grab the task.
	CTaskComplexControlMovement* pTask = static_cast<CTaskComplexControlMovement *>(pSubTask);
	
	//Create the holster task.
	CTask* pHolsterTask = rage_new CTaskSwapWeapon(SWAP_HOLSTER);
	
	//Start the holster task.
	pTask->SetNewMainSubtask(pHolsterTask);
	
	//Set the flag.
	m_uRunningFlags.SetFlag(RF_HasHolsteredWeapon);
}

void CTaskDraggingToSafety::UpdateMoveTaskForApproach()
{
	//Grab the nav mesh.
	CTaskMoveFollowNavMesh* pNavMesh = GetSubTaskNavMesh();
	if(!pNavMesh)
	{
		return;
	}
	
	//Calculate the target.
	Vec3V vTargetPosition;
	float fTargetHeading;
	CalculateApproachTargetAndHeading(RC_VECTOR3(vTargetPosition), fTargetHeading);

	//Set the target stop heading.
	pNavMesh->SetTargetStopHeading(fTargetHeading);

	//Grab the current target position.
	Vec3V vCurrentTargetPosition = VECTOR3_TO_VEC3V(pNavMesh->GetTarget());

	//Check if the distance between target positions exceeds the threshold.
	ScalarV scDistSq = DistSquared(vCurrentTargetPosition, vTargetPosition);
	ScalarV scMinDistSq = ScalarVFromF32(square(sm_Tunables.m_MinDistanceToSetApproachPosition));
	if(IsGreaterThanAll(scDistSq, scMinDistSq))
	{
		//Set the target.
		pNavMesh->SetTarget(GetPed(), vTargetPosition, true);
	}
}

float CTaskDraggingToSafety::CoverPointScoringCB(const CCoverPoint* pCoverPoint, const Vector3& vCoverPointCoords, void* pData)
{
	//The lower the score, the better the cover point.
	//Cover points are scored by:
	//	1) Distance
	//	2) Whether there is an opening to the left/right (middle of a wall is preferred)
	//	3) Amount of cover provided (based on dot)
	
	//Grab the cover point data.
	DraggingCoverPointData* pCoverData = (DraggingCoverPointData *)pData;
	
	//Grab the distance.
	float fDistance = pCoverData->m_vPosition.Dist(vCoverPointCoords);
	
	//Generate the distance score.
	float fDistanceScore = (fDistance - sm_Tunables.m_CoverMinDistance) / (sm_Tunables.m_CoverMaxDistance - sm_Tunables.m_CoverMinDistance);
	
	//Generate the usage score.
	float fUsageScore = 1.0f;
	switch(pCoverPoint->GetUsage())
	{
		case CCoverPoint::COVUSE_WALLTOLEFT:
		{
			fUsageScore = 0.25f;
			break;
		}
		case CCoverPoint::COVUSE_WALLTORIGHT:
		{
			fUsageScore = 0.25f;
			break;
		}
		case CCoverPoint::COVUSE_WALLTOBOTH:
		{
			fUsageScore = 0.0f;
			break;
		}
		case CCoverPoint::COVUSE_WALLTONEITHER:
		{
			fUsageScore = 1.0f;
			break;
		}
		default:
		{
			fUsageScore = 1.0f;
			break;
		}
	}
	
	//Generate the value of cover provided.
	float fValueScore = 0.0f;
	if(pCoverData->m_bUseTargetPosition)
	{
		//Generate a vector from the target to the cover point.
		Vector3 vTargetToCover = vCoverPointCoords - pCoverData->m_vTargetPosition;
		vTargetToCover.z = 0.0f;
		vTargetToCover.NormalizeSafe(ORIGIN);
		
		//Generate the cover direction vector.
		Vector3 vCoverDir = VEC3V_TO_VECTOR3(pCoverPoint->GetCoverDirectionVector());
		vCoverDir.z = 0.0f;
		vCoverDir.NormalizeSafe(ORIGIN);
		
		//Generate the dot product between the vectors.
		float fDot = vTargetToCover.Dot(vCoverDir);
		
		//Generate the value score from the dot product.
		fValueScore = (fDot + 1.0f) * 0.5f;
	}
	
	//Accumulate the score based on the weights.
	float fScore = 0.0f;
	fScore += (fDistanceScore * sm_Tunables.m_CoverWeightDistance);
	fScore += (fUsageScore * sm_Tunables.m_CoverWeightUsage);
	fScore += (fValueScore * sm_Tunables.m_CoverWeightValue);
	
	return fScore;
}

bool CTaskDraggingToSafety::CoverPointValidityCB(const CCoverPoint* pCoverPoint, const Vector3& vCoverPointCoords, void* pData)
{
	//Ensure another ped can be accomodated.
	if(!pCoverPoint->CanAccomodateAnotherPed())
	{
		return false;
	}
	
	//Ignore transient cover points.
	if(pCoverPoint->GetFlag(CCoverPoint::COVFLAGS_Transient))
	{
		return false;
	}
	
	//Grab the cover point data.
	DraggingCoverPointData* pCoverData = (DraggingCoverPointData *)pData;
	
	//Find the distance squared between the cover position and the ped position.
	float fDistSq = pCoverData->m_vPosition.Dist2(vCoverPointCoords);

	//Invalidate cover points outside of the distance range.
	float sMinDistSq = rage::square(sm_Tunables.m_CoverMinDistance);
	float sMaxDistSq = rage::square(sm_Tunables.m_CoverMaxDistance);
	if((fDistSq < sMinDistSq) || (fDistSq > sMaxDistSq))
	{
		return false;
	}
	
	//Check if the target position should be used.
	if(pCoverData->m_bUseTargetPosition)
	{
		//Generate a vector from the ped to the target.
		Vector3 vDraggedToTarget = pCoverData->m_vTargetPosition - pCoverData->m_vPosition;
		
		//Generate a vector from the dragged ped to the cover point.
		Vector3 vDraggedToCover = vCoverPointCoords - pCoverData->m_vPosition;
		
		//Invalidate cover points that move the dragged ped towards the target.
		if(DotProduct(vDraggedToTarget, vDraggedToCover) > 0.0f)
		{
			return false;
		}
	}
	
	//Grab the ped.
	CPed* pPed = pCoverData->m_pPed;
	
	//Grab the dragged ped.
	CPed* pDragged = pCoverData->m_pDragged;
	
	//Ensure there are not too many peds very close to the cover point.
	int iNumPedsVeryCloseToCoverPoint = 0;
	CEntityScannerIterator entityList = pPed->GetPedIntelligence()->GetNearbyPeds();
	for(const CEntity* pEntity = entityList.GetFirst(); pEntity; pEntity = entityList.GetNext())
	{
		//Ensure the entity is valid.
		if(!pEntity)
		{
			continue;
		}
		
		//Ensure the entity does not match the ped.
		if(pEntity == pPed)
		{
			continue;
		}
		
		//Ensure the entity does not match the dragged ped.
		if(pEntity == pDragged)
		{
			continue;
		}
		
		//Check if the nearby ped is very close to the cover point.
		Vec3V vNearbyPedPosition = pEntity->GetTransform().GetPosition();
		ScalarV scDistSq = DistSquared(vNearbyPedPosition, VECTOR3_TO_VEC3V(vCoverPointCoords));
		ScalarV scMaxDistSq = ScalarVFromF32(square(sm_Tunables.m_MaxDistanceForPedToBeVeryCloseToCover));
		if(IsGreaterThanAll(scDistSq, scMaxDistSq))
		{
			continue;
		}
		
		//Increase the number of peds that are very close.
		++iNumPedsVeryCloseToCoverPoint;
		if(iNumPedsVeryCloseToCoverPoint > sm_Tunables.m_MaxNumPedsAllowedToBeVeryCloseToCover)
		{
			return false;
		}
	}
	
	return true;
}

#if !__FINAL
void CTaskDraggingToSafety::Debug() const
{
#if DEBUG_DRAW
	//Add a marker above the dragging ped's head.
	grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()) + Vector3(0.0f, 0.0f, 1.0f), 0.1f, Color_purple);
#endif
}
#endif // !__FINAL

#if !__FINAL
const char * CTaskDraggingToSafety::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Start&&iState<=State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Stream",
		"State_CallForCover",
		"State_Approach",
		"State_PickupEntry",
		"State_Pickup",
		"State_Dragging",
		"State_Adjust",
		"State_Putdown",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL
