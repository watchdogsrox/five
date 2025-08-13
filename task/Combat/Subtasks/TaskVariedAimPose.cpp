// FILE :    TaskVariedAimPose.cpp
// PURPOSE : Combat subtask for varying a ped's aim pose
// CREATED : 09-21-2011

// File header
#include "Task/Combat/Subtasks/TaskVariedAimPose.h"

// Rage headers
#include "profile/profiler.h"

// Game headers
#include "Event/EventWeapon.h"
#include "Peds/ped.h"
#include "Peds/PedIntelligence.h"
#include "Task/Motion/TaskMotionBase.h"
#include "Task/Motion/Locomotion/TaskMotionAiming.h"
#include "Task/Movement/TaskMoveWithinAttackWindow.h"
#include "task/Weapons/Gun/TaskGun.h"
#include "Task/Combat/TaskNewCombat.h"
#include "Task/Combat/Subtasks/TaskShootAtTarget.h"

namespace AICombatStats
{
	EXT_PF_TIMER(CTaskVariedAimPose);
}
using namespace AICombatStats;

AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////
// CTaskVariedAimPose
////////////////////////////////////////////////////////////////////////////////

CTaskVariedAimPose::Tunables CTaskVariedAimPose::sm_Tunables;
IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskVariedAimPose, 0x248ae0ad);

CTaskVariedAimPose::CTaskVariedAimPose(CTask* pShootTask)
: m_ClipSetRequestHelper()
, m_AsyncProbeHelper()
, m_NavMeshLineOfSightHelper()
, m_vNewPosition(V_ZERO)
, m_vPositionToMoveAwayFrom(V_ZERO)
, m_Transition()
, m_TransitionShuffler(1)
, m_ClipShuffler(1)
, m_Reaction()
, m_pShootTask(pShootTask)
, m_pPose(NULL)
, m_pInitialPose(NULL)
, m_fClipDuration(0.0f)
, m_fTimeSinceLastReaction(LARGE_FLOAT)
, m_fTimeSinceLastThreatened(LARGE_FLOAT)
, m_fTimeSinceLastReactionCheckForGunAimedAt(LARGE_FLOAT)
, m_iTransitionCounter(-1)
, m_iClipCounter(-1)
, m_iClipsCheckedThisFrame(0)
, m_uConfigFlags(0)
, m_uRunningFlags(0)
{
	SetInternalTaskType(CTaskTypes::TASK_VARIED_AIM_POSE);
}

CTaskVariedAimPose::~CTaskVariedAimPose()
{
	//Free the shoot task.
	CTask* pShootTask = m_pShootTask.Get();
	delete pShootTask;
}

void CTaskVariedAimPose::CleanUp()
{
	PF_FUNC(CTaskVariedAimPose);

	//Clean up the pose.
	CleanUpPose();
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVariedAimPose::ProcessPreFSM()
{
	PF_FUNC(CTaskVariedAimPose);

	const CPed* pPed = GetPed();
	taskAssert(pPed);

	if (!pPed->IsNetworkClone())
	{
		//Process the timers.
		ProcessTimers();

		//Process the counters.
		ProcessCounters();

		//Process the flags.
		ProcessFlags();
	}

	//Process the heading.
	ProcessHeading();
		
	return FSM_Continue;
}

CTask::FSM_Return CTaskVariedAimPose::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{	
	PF_FUNC(CTaskVariedAimPose);

	FSM_Begin
	
		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter();
			FSM_OnUpdate
				return Start_OnUpdate();
				
		FSM_State(State_Shoot)
			FSM_OnEnter
				Shoot_OnEnter();
			FSM_OnUpdate
				return Shoot_OnUpdate();
				
		FSM_State(State_ChooseTransition)
			FSM_OnEnter
				ChooseTransition_OnEnter();
			FSM_OnUpdate
				return ChooseTransition_OnUpdate();
				
		FSM_State(State_CheckNextTransition)
			FSM_OnUpdate
				return CheckNextTransition_OnUpdate();
				
		FSM_State(State_StreamClipSet)
			FSM_OnUpdate
				return StreamClipSet_OnUpdate();
				
		FSM_State(State_ChooseClip)
			FSM_OnEnter
				ChooseClip_OnEnter();
			FSM_OnUpdate
				return ChooseClip_OnUpdate();
				
		FSM_State(State_CheckNextClip)
			FSM_OnUpdate
				return CheckNextClip_OnUpdate();
				
		FSM_State(State_CheckLineOfSight)
			FSM_OnEnter
				CheckLineOfSight_OnEnter();
			FSM_OnUpdate
				return CheckLineOfSight_OnUpdate();
				
		FSM_State(State_CheckNavMesh)
			FSM_OnEnter
				CheckNavMesh_OnEnter();
			FSM_OnUpdate
				return CheckNavMesh_OnUpdate();

		FSM_State(State_CheckProbe)
			FSM_OnEnter
				CheckProbe_OnEnter();
			FSM_OnUpdate
				return CheckProbe_OnUpdate();
			FSM_OnExit
				CheckProbe_OnExit();
				
		FSM_State(State_PlayTransition)
			FSM_OnEnter
				PlayTransition_OnEnter();
			FSM_OnUpdate
				return PlayTransition_OnUpdate();
			FSM_OnExit
				PlayTransition_OnExit();
				
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
			
	FSM_End
}

void CTaskVariedAimPose::Start_OnEnter()
{
	//Cache the initial pose.
	s32 iInitialPoseIdx = FindAimPoseIdx(GetPed()->GetIsCrouching() ? sm_Tunables.m_DefaultCrouchingPose.GetHash() : sm_Tunables.m_DefaultStandingPose.GetHash());
	taskAssertf(iInitialPoseIdx >= 0, "CTaskVariedAimPose::Start_OnEnter, Initial pose idx is invalid");
	m_pInitialPose = GetAimPoseAt(iInitialPoseIdx);
	taskAssertf(m_pInitialPose, "CTaskVariedAimPose::Start_OnEnter, Initial pose is invalid.");
	
	//Initialize the pose.
	ChangePose(iInitialPoseIdx);
	
	//Start the shoot task.
	SetNewTask(m_pShootTask);
	
	//The shoot task memory is now managed by this task.
	m_pShootTask = NULL;
}

CTask::FSM_Return CTaskVariedAimPose::Start_OnUpdate()
{
	//Move to the shoot state.
	SetStateAndKeepSubTask(State_Shoot);

	return FSM_Continue;
}

void CTaskVariedAimPose::Shoot_OnEnter()
{
}

CTask::FSM_Return CTaskVariedAimPose::Shoot_OnUpdate()
{
	//Check if the task has completed.
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//End the task.
		SetState(State_Finish);
	}
	else if(ShouldChooseNewPose())
	{
		//Choose a transition.
		SetStateAndKeepSubTask(State_ChooseTransition);
	}
	
	return FSM_Continue;
}

void CTaskVariedAimPose::ChooseTransition_OnEnter()
{
	//Release the clip set.
	m_ClipSetRequestHelper.ReleaseClipSet();
	
	//Clear the clip duration.
	m_fClipDuration = 0.0f;
	
	//Set the number of elements on the shuffler.
	m_TransitionShuffler.SetNumElements(m_pPose->m_Transitions.GetCount());
	
	//Initialize the counter.
	m_iTransitionCounter = 0;
	
	//Clear the failure flag.
	m_uRunningFlags.ClearFlag(RF_FailedToFixLineOfSight);
}

CTask::FSM_Return CTaskVariedAimPose::ChooseTransition_OnUpdate()
{
	//Check if the task has completed.
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//End the task.
		SetState(State_Finish);
	}
	else
	{
		//Check the next transition.
		SetStateAndKeepSubTask(State_CheckNextTransition);
	}
	
	return FSM_Continue;
}

CTask::FSM_Return CTaskVariedAimPose::CheckNextTransition_OnUpdate()
{
	//Check if the sub-task has finished.
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//End the task.
		SetState(State_Finish);
	}
	else
	{
		//Iterate over the transitions.
		while(m_iTransitionCounter < m_TransitionShuffler.GetNumElements())
		{
			//Generate the transition index.
			int iTransitionIndex = m_TransitionShuffler.GetElement(m_iTransitionCounter);
			++m_iTransitionCounter;
			
			//Ensure we can choose the transition.
			const AimPose::Transition& rTransition = m_pPose->m_Transitions[iTransitionIndex];
			if(!CanChooseTransition(rTransition))
			{
				continue;
			}
			
			//Set the transition.
			SetTransition(iTransitionIndex);
			
			//Check if the transition has a specific clip.
			bool bDoesTransitionHaveSpecificClip = (m_Transition.m_ClipId != CLIP_ID_INVALID);
			m_uRunningFlags.ChangeFlag(RF_DoesTransitionHaveSpecificClip, bDoesTransitionHaveSpecificClip);
			
			//Stream the clip set.
			SetStateAndKeepSubTask(State_StreamClipSet);
			
			return FSM_Continue;
		}
		
		//Note that we are done choosing a transition.
		DoneChoosingTransition(false);
		
		//No suitable transition was found, go back to shooting.
		SetStateAndKeepSubTask(State_Shoot);
	}
	
	return FSM_Continue;
}

CTask::FSM_Return CTaskVariedAimPose::StreamClipSet_OnUpdate()
{
	//Check if the sub-task has finished.
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//End the task.
		SetState(State_Finish);
	}
	else
	{
		//Check if the clip set is invalid.
		if(m_Transition.m_ClipSetId == CLIP_SET_ID_INVALID)
		{
			//Check the next transition.
			SetStateAndKeepSubTask(State_CheckNextTransition);
		}
		else
		{
			//Request the clip set.
			m_ClipSetRequestHelper.RequestClipSet(m_Transition.m_ClipSetId);

			//Check if the clip set should be streamed out.
			if(m_ClipSetRequestHelper.ShouldClipSetBeStreamedOut())
			{
				//Check the next transition.
				SetStateAndKeepSubTask(State_CheckNextTransition);
			}
			//Check if the clip set is streamed in.
			else if(m_ClipSetRequestHelper.ShouldClipSetBeStreamedIn() && m_ClipSetRequestHelper.IsClipSetStreamedIn())
			{
				//Choose a clip.
				SetStateAndKeepSubTask(State_ChooseClip);
			}
		}
	}

	return FSM_Continue;
}

void CTaskVariedAimPose::ChooseClip_OnEnter()
{
	//Grab the clip set.
	fwClipSet* pClipSet = m_ClipSetRequestHelper.GetClipSet();
	taskAssertf(pClipSet, "The clip set is invalid.");
	
	//Set the number of elements on the shuffler.
	int iNumElements = pClipSet->GetClipItemCount();
	m_ClipShuffler.SetNumElements(iNumElements > 0 ? iNumElements : 1);
	
	//Initialize the counter.
	m_iClipCounter = 0;
}

CTask::FSM_Return CTaskVariedAimPose::ChooseClip_OnUpdate()
{
	//Check if the sub-task has finished.
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//End the task.
		SetState(State_Finish);
	}
	else
	{
		//Check the next clip.
		SetStateAndKeepSubTask(State_CheckNextClip);
	}
	
	return FSM_Continue;
}

CTask::FSM_Return CTaskVariedAimPose::CheckNextClip_OnUpdate()
{
	//Check if the sub-task has finished.
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//End the task.
		SetState(State_Finish);
	}
	//Check if the clip set should be streamed out.
	else if(m_ClipSetRequestHelper.ShouldClipSetBeStreamedOut())
	{
		//Check the next transition.
		SetState(State_CheckNextTransition);
	}
	//Check if the clip set is not streamed in.
	else if(!m_ClipSetRequestHelper.IsClipSetStreamedIn())
	{
		//Check the next transition.
		SetState(State_CheckNextTransition);
	}
	else
	{
		//Grab the clip set.
		fwClipSet* pClipSet = m_ClipSetRequestHelper.GetClipSet();
		taskAssertf(pClipSet, "The clip set is invalid.");
		
		//Iterate over the clips.
		while(m_iClipCounter < m_ClipShuffler.GetNumElements())
		{
			//Check if we have checked too many clips this frame.
			if(m_iClipsCheckedThisFrame > sm_Tunables.m_MaxClipsToCheckPerFrame)
			{
				return FSM_Continue;
			}
			
			//Increase the number of clips checked this frame.
			++m_iClipsCheckedThisFrame;
	
			//Generate the clip index.
			int iClipIndex = m_ClipShuffler.GetElement(m_iClipCounter);
			++m_iClipCounter;
			
			//Check if the transition has a specific clip.
			bool bDoesTransitionHaveSpecificClip = m_uRunningFlags.IsFlagSet(RF_DoesTransitionHaveSpecificClip);
			if(bDoesTransitionHaveSpecificClip)
			{
				//Check if we have already checked the clip.
				if(m_iClipCounter > 1)
				{
					break;
				}

				UpdateLocalClipIdFromTransition();
			}
			else
			{
				//Set the clip.
				SetTransitionClipId(pClipSet->GetClipItemIdByIndex(iClipIndex));
				taskAssertf(m_Transition.m_ClipId != CLIP_ID_INVALID, "The clip is invalid for clip index: %d.", iClipIndex);
			}
			
			//Ensure the clip is valid.
			const crClip* pClip = pClipSet->GetClip(m_Transition.m_ClipId);
			if(!taskVerifyf(pClip, "The clip is invalid: %s.", m_Transition.m_ClipId.GetCStr()))
			{
				continue;
			}
			
			//Ensure we can choose the clip.
			Vec3V vClipOffset;
			if(!CanChooseClip(*pClip, vClipOffset))
			{
				continue;
			}
			
			//Set the clip duration.
			m_fClipDuration = pClip->GetDuration();
			
			//Check if the offset is large enough to matter.
			if(IsOffsetLargeEnoughToMatter(vClipOffset))
			{
				//Check if we are moving away from a position.
				if(HasPositionToMoveAwayFrom())
				{
					//Set the flag.
					m_uRunningFlags.SetFlag(RF_MovingAwayFromPosition);
				}
				
				//Set the new position.
				m_vNewPosition = Add(GetPed()->GetTransform().GetPosition(), vClipOffset);
				
				//Check the line of sight.
				SetStateAndKeepSubTask(State_CheckLineOfSight);
				
				return FSM_Continue;
			}
			else
			{
				//Note that we are done choosing a transition.
				DoneChoosingTransition(true);
				
				//Play the transition.
				SetStateAndKeepSubTask(State_PlayTransition);
				
				return FSM_Continue;
			}
		}
		
		//Check the next transition.
		SetStateAndKeepSubTask(State_CheckNextTransition);
	}
	
	return FSM_Continue;
}

void CTaskVariedAimPose::CheckLineOfSight_OnEnter()
{
	//Start a line of sight test from the new position to the target.
	StartTestLineOfSight(m_vNewPosition);
}

CTask::FSM_Return CTaskVariedAimPose::CheckLineOfSight_OnUpdate()
{
	//Check if the sub-task has finished.
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//End the task.
		SetState(State_Finish);
	}
	//Check if the line of sight test is active.
	else if(m_AsyncProbeHelper.IsProbeActive())
	{
		//Get the probe results.
		ProbeStatus eProbeStatus;
		if(m_AsyncProbeHelper.GetProbeResults(eProbeStatus) && (eProbeStatus != PS_WaitingForResult))
		{
			//Check if the line of sight is clear.
			if(eProbeStatus == PS_Clear)
			{
				//Check the nav mesh.
				SetStateAndKeepSubTask(State_CheckNavMesh);
			}
			else
			{
				//Check the next clip.
				SetStateAndKeepSubTask(State_CheckNextClip);
			}
		}	
	}
	else
	{
		//Check the next clip.
		SetStateAndKeepSubTask(State_CheckNextClip);
	}
	
	return FSM_Continue;
}

void CTaskVariedAimPose::CheckNavMesh_OnEnter()
{
	//Start a nav mesh line of sight test from the current position to the new position.
	m_NavMeshLineOfSightHelper.StartLosTest(VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()), VEC3V_TO_VECTOR3(m_vNewPosition), 0.0f, true);
}

CTask::FSM_Return CTaskVariedAimPose::CheckNavMesh_OnUpdate()
{
	//Check if the sub-task has finished.
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//End the task.
		SetState(State_Finish);
	}
	//Check if the nav mesh test is active.
	else if(m_NavMeshLineOfSightHelper.IsTestActive())
	{
		//Get the results.
		SearchStatus searchResult;
		bool bLosIsClear;
		bool bNoSurfaceAtCoords;
		if(m_NavMeshLineOfSightHelper.GetTestResults(searchResult, bLosIsClear, bNoSurfaceAtCoords, 1, NULL) && (searchResult != SS_StillSearching))
		{
			//Check if the line of sight is clear.
			if(bLosIsClear)
			{
				//Check the probe.
				SetStateAndKeepSubTask(State_CheckProbe);
			}
			else
			{
				//Check the next clip.
				SetStateAndKeepSubTask(State_CheckNextClip);
			}
		}
	}
	else
	{
		//Check the next clip.
		SetStateAndKeepSubTask(State_CheckNextClip);
	}
	
	return FSM_Continue;
}

void CTaskVariedAimPose::CheckProbe_OnEnter()
{
	//Cache the radius.
	static dev_float s_fRadius = 0.25f;
	float fRadius = s_fRadius;

	//Create the probe.
	WorldProbe::CShapeTestCapsuleDesc probeDesc;
	probeDesc.SetIsDirected(true);
	probeDesc.SetResultsStructure(&m_ProbeResults);
	probeDesc.SetCapsule(VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()), VEC3V_TO_VECTOR3(m_vNewPosition), fRadius);
	probeDesc.SetExcludeEntity(GetPed());
	probeDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_PED_TYPE |
		ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE);

	//Start the test.
	WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
}

CTask::FSM_Return CTaskVariedAimPose::CheckProbe_OnUpdate()
{
	//Check if the probe failed.
	if(!m_ProbeResults.GetResultsWaitingOrReady())
	{
		//Check the next clip.
		SetStateAndKeepSubTask(State_CheckNextClip);
	}
	//Check if the results are ready.
	else if(m_ProbeResults.GetResultsReady())
	{
		//Check if we did not hit anything.
		if(m_ProbeResults.GetNumHits() == 0)
		{
			//Note that we are done choosing a transition.
			DoneChoosingTransition(true);

			//Play the transition.
			SetStateAndKeepSubTask(State_PlayTransition);
		}
		else
		{
			//Check the next clip.
			SetStateAndKeepSubTask(State_CheckNextClip);
		}
	}

	return FSM_Continue;
}

void CTaskVariedAimPose::CheckProbe_OnExit()
{
	//Reset the results.
	m_ProbeResults.Reset();
}

void CTaskVariedAimPose::PlayTransition_OnEnter()
{
	//Check if we can player the transition.
	if(CanPlayTransition())
	{
		SetTransitionPlayStatus(CSerializableState::STransitionPlayStatus::ACCEPTED);

		//Check if the transition played successfully.
		if(taskVerifyf(PlayTransition(m_Transition), "CTaskVariedAimPose::PlayTransition_OnEnter, PlayTransition failed!"))
		{
			//Nothing else to do.
			return;
		}
	}

	//At this point, the transition has failed.
	SetTransitionPlayStatus(CSerializableState::STransitionPlayStatus::REJECTED);

	//Clear the clip duration.
	m_fClipDuration = 0.0f;
}

CTask::FSM_Return CTaskVariedAimPose::PlayTransition_OnUpdate()
{
	//Check if the time in state has exceeded the clip duration.
	if(GetTimeInState() >= m_fClipDuration)
	{
		if (!GetPed()->IsNetworkClone())
		{
			//Change the pose.
			ChangePose(FindAimPoseIdx(m_Transition.m_ToPose.GetHash()));
			taskAssertf(m_pPose, "The pose is invalid: %s.", m_Transition.m_ToPose.GetCStr());
		}

		//Move back to the shoot task.
		SetStateAndKeepSubTask(State_Shoot);
	}

	return FSM_Continue;
}

void CTaskVariedAimPose::PlayTransition_OnExit()
{
	//Check if the ped was blocking someone else's line of sight.
	if(m_uRunningFlags.IsFlagSet(RF_IsBlockingLineOfSight))
	{
		//Clear the flag.
		m_uRunningFlags.ClearFlag(RF_IsBlockingLineOfSight);
	}
	
	//Check if we were moving away from a position.
	if(HasPositionToMoveAwayFrom() && m_uRunningFlags.IsFlagSet(RF_MovingAwayFromPosition))
	{
		//Clear the position to move away from.
		m_vPositionToMoveAwayFrom = Vec3V(V_ZERO);
	}
	
	//Release the clip set.
	m_ClipSetRequestHelper.ReleaseClipSet();

	//Clear the flag
	m_uRunningFlags.ClearFlag(RF_MovingAwayFromPosition);

	//Clear played transition
	m_LocalSerializableState.ClearTransition();

	//Reset transition received from network
	m_FromNetworkSerializableState.ClearTransition();
}

////////////////////////////////////////////////////////////////////////////////

void CTaskVariedAimPose::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(State_Finish);
}

////////////////////////////////////////////////////////////////////////////////

CTaskInfo* CTaskVariedAimPose::CreateQueriableState() const
{
	return rage_new CClonedVariedAimPoseInfo(GetState(), m_LocalSerializableState);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskVariedAimPose::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	Assert( pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_VARIED_AIM_POSE);
	CClonedVariedAimPoseInfo* pVariedAimPoseInfo = static_cast<CClonedVariedAimPoseInfo*>(pTaskInfo);

	m_FromNetworkSerializableState = pVariedAimPoseInfo->GetPoseTransitionInfo();

	CTaskFSMClone::ReadQueriableState(pTaskInfo);	
}

////////////////////////////////////////////////////////////////////////////////

CTaskVariedAimPose::FSM_Return CTaskVariedAimPose::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	if (GetStateFromNetwork() == State_Finish && GetState() != State_Finish && iEvent == OnUpdate)
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

#if __BANK
	// Debug logging for url:bugstar:2326868.
	// We update the pose in Shoot_OnUpdate_Clone. 
	// Print out debug info if we're not in this state to update the pose, but the pose idx is different.
	if (iState != State_Start && iState != State_Shoot
		&& (m_LocalSerializableState.GetPoseIdx() == -1 
			|| m_LocalSerializableState.GetPoseIdx() != m_FromNetworkSerializableState.GetPoseIdx()))
	{
		CPed *pPed = GetPed();
		s32 iPedIndex = pPed ? CPed::GetPool()->GetIndex(pPed) : -1;
		Displayf("CTaskVariedAimPose::UpdateClonedFSM: Poses should be the same! Ped: %d, iState: %d, Prev State: %d, m_LocalSerializableState.GetPoseIdx(): %d, m_FromNetworkSerializableState.GetPoseIdx(): %d", iPedIndex, iState, GetPreviousState(), m_LocalSerializableState.GetPoseIdx(), m_FromNetworkSerializableState.GetPoseIdx());
	}
#endif

FSM_Begin

	FSM_State(State_Start)
		FSM_OnUpdate
		return Start_OnUpdate_Clone();

	FSM_State(State_Shoot)
		FSM_OnUpdate
		return Shoot_OnUpdate_Clone();

	FSM_State(State_StreamClipSet)
		FSM_OnUpdate
		return StreamClipSet_OnUpdate_Clone();

	FSM_State(State_PlayTransition)
		FSM_OnEnter
		PlayTransition_OnEnter_Clone();
	FSM_OnUpdate
		return PlayTransition_OnUpdate_Clone();
	FSM_OnExit
		PlayTransition_OnExit();

	FSM_State(State_Finish)
		FSM_OnUpdate
		return FSM_Quit;

FSM_End
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVariedAimPose::Start_OnUpdate_Clone()
{
	CTaskFSMClone::CreateSubTaskFromClone(GetPed(), CTaskTypes::TASK_GUN);

	SetStateAndKeepSubTask(State_Shoot);

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVariedAimPose::Shoot_OnUpdate_Clone()
{
	UpdatePoseFromNetwork();

	switch (GetStateFromNetwork())
	{
	case State_Shoot: 
	case State_ChooseTransition:
	case State_CheckNextTransition:
		{
			if (!GetSubTask())
			{
				CTaskFSMClone::CreateSubTaskFromClone(GetPed(), CTaskTypes::TASK_GUN);
			}

			// Nothing to do
		}
		break;

	case State_StreamClipSet:
	case State_ChooseClip:
	case State_CheckNextClip:
	case State_CheckLineOfSight:
	case State_CheckNavMesh:
	case State_CheckProbe:
	case State_PlayTransition:
		{
			SetStateAndKeepSubTask(State_StreamClipSet);
		}
		break;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVariedAimPose::StreamClipSet_OnUpdate_Clone()
{
	if (GetStateFromNetwork() == State_Shoot)
	{
		// We are far too late or the transition was canceled on the local ped
		CancelClonePlayTransition();
		return FSM_Continue;
	}

	UpdateFromNetwork();

	if (m_LocalSerializableState.GetTransitionIdx() >= 0 && taskVerifyf(m_Transition.m_ClipSetId != CLIP_SET_ID_INVALID, "Clipset should be valid for a valid transition idx"))
	{
		//Request the clip set.
		m_ClipSetRequestHelper.RequestClipSet(m_Transition.m_ClipSetId);

		//If we have the clipset, set the ClipId and 
		const fwClipSet* pClipSet = m_ClipSetRequestHelper.GetClipSet();
		if (pClipSet)
		{
			//Should we try to play the transition?
			if (GetStateFromNetwork() == State_PlayTransition &&
				m_LocalSerializableState.GetTransitionClipId() != CLIP_ID_INVALID && 
				m_LocalSerializableState.GetTransitionPlayStatus() != CSerializableState::STransitionPlayStatus::UNDEFINED &&
				taskVerifyf(m_Transition.m_ClipId != CLIP_ID_INVALID, "Clip should be valid for a valid transition clip idx") )
			{
				//Check if the clip set is streamed in and we have chosen a clip id.
				if(!m_ClipSetRequestHelper.ShouldClipSetBeStreamedOut() && 
					m_ClipSetRequestHelper.ShouldClipSetBeStreamedIn() && 
					m_ClipSetRequestHelper.IsClipSetStreamedIn())
				{
					//Init clip duration
					const crClip* pClip = pClipSet->GetClip(m_Transition.m_ClipId);
					if (taskVerifyf(pClip, "CTaskVariedAimPose::StreamClipSet_OnUpdate_Clone, Null clip!"))
					{
						m_fClipDuration = pClip->GetDuration();
					}

					//Play the transition.
					SetStateAndKeepSubTask(State_PlayTransition);
				}
			}
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskVariedAimPose::PlayTransition_OnEnter_Clone()
{
	taskAssert(m_LocalSerializableState.GetTransitionPlayStatus() != CSerializableState::STransitionPlayStatus::UNDEFINED);
	if (m_LocalSerializableState.GetTransitionPlayStatus() == CSerializableState::STransitionPlayStatus::ACCEPTED)
	{
		//Check if the transition played successfully.
		if(taskVerifyf(PlayTransition(m_Transition), "CTaskVariedAimPose::PlayTransition_OnEnter, PlayTransition failed!"))
		{
			//Nothing else to do.
			return;
		}
	}

	//Clear the clip duration.
	m_fClipDuration = 0.0f;
}

////////////////////////////////////////////////////////////////////////////////

CTaskVariedAimPose::FSM_Return CTaskVariedAimPose::PlayTransition_OnUpdate_Clone()
{
	// Force finish transition when local has already finished
	if (GetStateFromNetwork() != State_PlayTransition)
	{
		m_fClipDuration = 0.0f;
	}

	return PlayTransition_OnUpdate();
}

////////////////////////////////////////////////////////////////////////////////

void CTaskVariedAimPose::CancelClonePlayTransition()
{
	// We are not getting the clipset id, the local ped has gone back to shoot
	m_LocalSerializableState.ClearTransition();

	//Reset transition received from network
	m_FromNetworkSerializableState.ClearTransition();

	SetStateAndKeepSubTask(State_Shoot);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskVariedAimPose::UpdateFromNetwork()
{
	UpdateTransitionFromNetwork();

	if (m_LocalSerializableState.GetTransitionIdx() >= 0)
	{
		UpdateTransitionClipIdFromNetwork();

		if (m_LocalSerializableState.GetTransitionClipId() != CLIP_ID_INVALID)
		{
			UpdateTransitionPlayStatusFromNetwork();
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskVariedAimPose::CanChooseClip(const crClip& rClip, Vec3V_InOut vClipOffset) const
{
	//Grab the clip offset in mover coordinate.
	Vector3 vClipOffsetLocal = fwAnimHelpers::GetMoverTrackDisplacement(rClip, 0.0f, 1.0f);

	//Transform the clip offset to world coordinates.
	vClipOffset = GetPed()->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(vClipOffsetLocal));
	
	//Check the if the offset is large enough to matter.
	bool bOffsetLargeEnoughToMatter = IsOffsetLargeEnoughToMatter(vClipOffset);
	if(bOffsetLargeEnoughToMatter)
	{
		//This section is for checks in which we do not want to use the transition, under ANY circumstances.
		
		//Ensure the offset is not outside the defensive area.
		if(IsOffsetOutsideDefensiveArea(vClipOffset))
		{
			return false;
		}

		//Ensure the offset is not outside the attack window.
		if(IsOffsetOutsideAttackWindow(vClipOffset))
		{
			return false;
		}

		//Ensure the offset is not towards the target (if the target is close).
		if(IsOffsetTowardsTarget(vClipOffset))
		{
			return false;
		}
		
		//Ensure the offset is not towards a nearby ped.
		if(IsOffsetTowardsNearbyPed(vClipOffset))
		{
			return false;
		}
	}
	
	//At this point, we have passed all global checks.
	
	//This section is for checks that are specific to the transition trigger.
	//Possible transition triggers are:
	//	1) Randomness
	//	2) Line of sight is blocked
	//	3) Blocking someone's line of sight
	//	4) Reacting to an event
	//	5) Moving away from a position

	//Check if the transition will result in the default standing pose.
	bool bTransitionWillResultInDefaultStandingPose = (m_Transition.m_ToPose == sm_Tunables.m_DefaultStandingPose);

	//Check if the transition will result in a crouch.
	s32 iAimPoseIdx = FindAimPoseIdx(m_Transition.m_ToPose.GetHash());
	taskAssert(iAimPoseIdx >= 0);
	bool bTransitionWillResultInCrouch = GetAimPoseAt(iAimPoseIdx)->m_IsCrouching;

	//Check if our line of sight is blocked.
	if(m_uRunningFlags.IsFlagSet(RF_IsLineOfSightBlocked))
	{
		//Check if the animation involves movement.
		if(bOffsetLargeEnoughToMatter)
		{
			//This pose is allowed.
		}
		//Check if the transition will result in the default standing pose.
		//When our line of sight is blocked, movement is required.
		//Unfortunately, only the default standing pose has animations that result in movement.
		//This means that if we are not in the default standing pose, we need to transition there before we can move.
		//This is a pretty massive hack that depends largely on the state of the metadata.
		else if(bTransitionWillResultInDefaultStandingPose)
		{
			//This pose is allowed.
		}
		else
		{
			return false;
		}
	}
	
	//Check if we are blocking someone else's line of sight.
	if(m_uRunningFlags.IsFlagSet(RF_IsBlockingLineOfSight))
	{
		//Check if the animation involves movement.
		if(bOffsetLargeEnoughToMatter)
		{
			//This pose is allowed.
		}
		else
		{
			//Check if we are crouching.
			if(m_pPose->m_IsCrouching)
			{
				//Check if the transition will not result in a crouch.
				if(!bTransitionWillResultInCrouch)
				{
					//Don't let us stand up in front of someone,
					//when we are blocking their line of sight.
					return false;
				}
			}
		}
	}
	
	//Check if we are reacting to an event.
	if(m_Reaction.m_bValid)
	{
		//Check if the offset is large enough to matter.
		if(bOffsetLargeEnoughToMatter)
		{
			//Ensure we are not moving towards the reaction position.
			if(IsOffsetTowardsPosition(vClipOffset, m_Reaction.m_vPos))
			{
				return false;
			}
		}
	}
	
	//Check if we are moving away from a position.
	if(HasPositionToMoveAwayFrom())
	{
		//If the offset is large enough to matter.
		if(bOffsetLargeEnoughToMatter)
		{
			//Ensure we are not moving towards the position to move away from.
			if(IsOffsetTowardsPosition(vClipOffset, m_vPositionToMoveAwayFrom))
			{
				return false;
			}
		}
		else
		{
			//Check if the transition will result in a crouch.
			if(bTransitionWillResultInCrouch)
			{
				if(!m_uRunningFlags.IsFlagSet(RF_IsBlockingLineOfSight))
				{
					return false;
				}
			}
			else if(!bTransitionWillResultInDefaultStandingPose)
			{
				return false;
			}
		}
	}

	return true;
}

bool CTaskVariedAimPose::CanChooseNewPose() const
{
	//Check if we should disable new poses from the default standing pose.
	if(m_uConfigFlags.IsFlagSet(CF_DisableNewPosesFromDefaultStandingPose))
	{
		//Check if we are in the default standing pose.
		if(IsInDefaultStandingPose())
		{
			return false;
		}
	}

	//Ensure the min time before a new pose has been exceeded.
	float fTimeInState = GetTimeInState();
	float fMinTimeBeforeNewPose = sm_Tunables.m_MinTimeBeforeCanChooseNewPose;
	if(fTimeInState < fMinTimeBeforeNewPose)
	{
		return false;
	}
	
	return true;
}

bool CTaskVariedAimPose::CanChooseTransition(const AimPose::Transition& rTransition) const
{
	//Ensure the pose is valid.
	const s32 iPoseIdx = FindAimPoseIdx(rTransition.m_ToPose.GetHash());
	if (iPoseIdx < 0)
	{
		return false;
	}

	const AimPose* pPose = GetAimPoseAt(iPoseIdx);
	if(!pPose)
	{
		return false;
	}

	//Check if the transition will result in the default standing pose.
	bool bTransitionWillResultInDefaultStandingPose = (rTransition.m_ToPose == sm_Tunables.m_DefaultStandingPose);

	//Check if transitions are disabled, except to the default standing pose.
	if(m_uConfigFlags.IsFlagSet(CF_DisableTransitionsExceptToDefaultStandingPose))
	{
		//Check if this transition will not result in the default standing pose.
		if(!bTransitionWillResultInDefaultStandingPose)
		{
			return false;
		}
	}

	//Check if transitions from the default standing pose are disabled.
	if(m_uConfigFlags.IsFlagSet(CF_DisableTransitionsFromDefaultStandingPose))
	{
		//Check if we are in the default standing pose.
		if(IsInDefaultStandingPose())
		{
			return false;
		}
	}
	
	//Check if we should use a reaction.
	bool bShouldUseReaction = m_Reaction.m_bValid;
	if(bShouldUseReaction)
	{
		//Ensure this transition can be used for reactions.
		bool bOnlyUseForReactions = rTransition.m_Flags.IsFlagSet(AimPose::Transition::OnlyUseForReactions);
		bool bCanUseForReactions = rTransition.m_Flags.IsFlagSet(AimPose::Transition::CanUseForReactions);
		if(!bOnlyUseForReactions && !bCanUseForReactions)
		{
			return false;
		}
	}
	else
	{
		//Ensure this transition is not only used for reactions.
		bool bOnlyUseForReactions = rTransition.m_Flags.IsFlagSet(AimPose::Transition::OnlyUseForReactions);
		if(bOnlyUseForReactions)
		{
			return false;
		}
	}
	
	//Check if we should use urgent transitions.
	bool bShouldUseUrgent = ShouldUseUrgentTransitions();
	if(bShouldUseUrgent)
	{
		//Check if the transition is not urgent.
		bool bIsUrgent = rTransition.m_Flags.IsFlagSet(AimPose::Transition::Urgent);
		if(!bIsUrgent)
		{
			//This is a bit of a hack, and is very dependent on the metadata.
			//Basically, we want to allow transitions to the standing pose through,
			//even though they are not urgent, because all of the urgent poses originate
			//from the default standing pose.
			if(!bTransitionWillResultInDefaultStandingPose)
			{
				return false;
			}
		}
	}
	else
	{
		//Ensure the transition is not urgent.
		bool bIsUrgent = rTransition.m_Flags.IsFlagSet(AimPose::Transition::Urgent);
		if(bIsUrgent)
		{
			return false;
		}
	}
	
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Check if this transition is only for law enforcement peds.
	bool bOnlyUseForLawEnforcementPeds = rTransition.m_Flags.IsFlagSet(AimPose::Transition::OnlyUseForLawEnforcementPeds);
	if(bOnlyUseForLawEnforcementPeds)
	{
		//Ensure the ped is law enforcement.
		if(!pPed->IsLawEnforcementPed())
		{
			return false;
		}
	}
	
	//Check if this transition is only for gang peds.
	bool bOnlyUseForGangPeds = rTransition.m_Flags.IsFlagSet(AimPose::Transition::OnlyUseForGangPeds);
	if(bOnlyUseForGangPeds)
	{
		//Ensure the ped is gang.
		if(!pPed->IsGangPed())
		{
			return false;
		}
	}
	
	//Check if we are not allowing crouched poses.
	if(pPose->m_IsCrouching)
	{
		//If not allowed to crouch then don't choose this pose
		if(!pPed->CanPedCrouch())
		{
			return false;
		}

		//If no crouched movement allowed, ensure the pose will not result in crouched movement.
		if(!CGameConfig::Get().AllowCrouchedMovement() && !pPose->m_IsStationary)
		{
			return false;
		}
	}
	
	return true;
}

bool CTaskVariedAimPose::CanPlayTransition() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Find the aiming task.
	CTaskMotionAiming* pTask = static_cast<CTaskMotionAiming *>(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_AIMING));
	if(!pTask)
	{
		return false;
	}

	//Ensure a transition can be played.
	if(!pTask->CanPlayTransition())
	{
		return false;
	}
	
	return true;
}

bool CTaskVariedAimPose::CanReact() const
{
	//Ensure we are in the shoot state.
	if(GetState() != State_Shoot)
	{
		return false;
	}
	
	//Peds can only react after a certain amount of time.
	float fMinTimeBetweenReactions = sm_Tunables.m_MinTimeBetweenReactions;
	if(m_fTimeSinceLastReaction < fMinTimeBetweenReactions)
	{
		return false;
	}
	
	//Ensure we can choose a new pose.
	if(!CanChooseNewPose())
	{
		return false;
	}
	
	return true;
}


void CTaskVariedAimPose::ChangePose(const s32 iPoseIdx)
{
	//Ensure the pose is changing.
	if(m_LocalSerializableState.GetPoseIdx() == iPoseIdx)
	{
		return;
	}
	
	//Assign the pose.
	m_LocalSerializableState.SetPoseIdx(iPoseIdx);

	if (!taskVerifyf(m_LocalSerializableState.GetPoseIdx() != -1, "The pose idx is invalid."))
	{
		return;
	}

	m_pPose = GetAimPoseAt(iPoseIdx);

	if(!taskVerifyf(m_pPose, "The pose is invalid."))
	{
		return;
	}

	//Set the crouching state.
	GetPed()->SetIsCrouching(m_pPose->m_IsCrouching);

	//
	m_LocalSerializableState.ClearTransition();
}

////////////////////////////////////////////////////////////////////////////////

void CTaskVariedAimPose::SetTransition(const s32 iTransitionIdx)
{
	if (!taskVerifyf(m_pPose, "CTaskVariedAimPose::SetTransition, cannot set a transition without a valid pose."))
	{
		return;
	}

	if (!taskVerifyf(iTransitionIdx >= 0 && iTransitionIdx < m_pPose->m_Transitions.GetCount(), "CTaskVariedAimPose::SetTransition, invalid transition index [%d].", iTransitionIdx))
	{
		return;
	}

	m_Transition = m_pPose->m_Transitions[iTransitionIdx];

	m_LocalSerializableState.SetTransitionIdx(iTransitionIdx);
	m_LocalSerializableState.ClearTransitionClipId();
}

////////////////////////////////////////////////////////////////////////////////

void CTaskVariedAimPose::SetTransitionClipId(const fwMvClipId& ClipId)
{
	m_Transition.m_ClipId = ClipId;
	UpdateLocalClipIdFromTransition();
}

////////////////////////////////////////////////////////////////////////////////

void CTaskVariedAimPose::SetTransitionPlayStatus(CSerializableState::STransitionPlayStatus::Enum ePlayStatus)
{
	m_LocalSerializableState.SetTransitionPlayStatus(ePlayStatus);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskVariedAimPose::UpdateLocalClipIdFromTransition()
{
	m_LocalSerializableState.SetTransitionClipId(m_Transition.m_ClipId);
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipId CTaskVariedAimPose::ChooseRandomClipInClipSet(fwMvClipSetId clipSetId) const
{
	//Ensure the clip set is valid.
	fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
	if(!taskVerifyf(pClipSet, "The clip set is invalid."))
	{
		return CLIP_ID_INVALID;
	}
	
	//Count the clips.
	int iCount = pClipSet->GetClipItemCount();
	
	//Choose a random index.
	int iIndex = fwRandom::GetRandomNumberInRange(0, iCount);
	
	return pClipSet->GetClipItemIdByIndex(iIndex);
}

bool CTaskVariedAimPose::CleanUpPose()
{
	//Ensure the initial pose is valid.
	if(!m_pInitialPose)
	{
		return false;
	}
	
	//Grab the ped.
	CPed* pPed = GetPed();
	
	//Reset the crouching state.
	pPed->SetIsCrouching(m_pInitialPose->m_IsCrouching);
	
	//Grab the motion data.
	CPedMotionData* pMotionData = pPed->GetMotionData();
	
	//Clear the stationary aim pose.
	pMotionData->GetStationaryAimPose().Clear();
	
	//Clear the aim pose transition.
	pMotionData->GetAimPoseTransition().Clear();
	
	return true;
}

void CTaskVariedAimPose::DoneChoosingTransition(bool bSucceeded)
{
	//Check if we succeeded.
	if(bSucceeded)
	{
		//Nothing to do.
	}
	else
	{
		//Check if our line of sight is blocked.
		if(m_uRunningFlags.IsFlagSet(RF_IsLineOfSightBlocked))
		{
			//Note that we were unable to fix our line of sight.
			m_uRunningFlags.SetFlag(RF_FailedToFixLineOfSight);
		}
	}
	
	//Clear the line of sight helper.
	m_AsyncProbeHelper.ResetProbe();
	
	//Clear the nav mesh helper.
	m_NavMeshLineOfSightHelper.ResetTest();
	
	//Clear the reaction.
	m_Reaction.Clear();
}

const CTaskVariedAimPose::AimPose* CTaskVariedAimPose::GetAimPoseAt(const s32 iPoseIdx) const
{
	if (taskVerifyf(iPoseIdx >= 0 && iPoseIdx < sm_Tunables.m_AimPoses.GetCount(), "CTaskVariedAimPose::GetAimPoseAt, Invalid pose idx [%d]", iPoseIdx))
	{
		return &sm_Tunables.m_AimPoses[iPoseIdx];
	}

	return NULL;
}

s32 CTaskVariedAimPose::FindAimPoseIdx(const u32 uName) const
{
	for(s32 i = 0; i < sm_Tunables.m_AimPoses.GetCount(); ++i)
	{
		//Grab the aim pose.
		const CTaskVariedAimPose::AimPose& rAimPose = sm_Tunables.m_AimPoses[i];
		if(rAimPose.m_Name.GetHash() == uName)
		{
			return i;
		}
	}
	
	return -1;
}

const CTaskVariedAimPose::AimPose::Transition* CTaskVariedAimPose::FindAimTransitionToPose(const u32 uName) const
{
	for(s32 i = 0; i < m_pPose->m_Transitions.GetCount(); ++i)
	{
		//Grab the transition.
		const AimPose::Transition& rTransition = m_pPose->m_Transitions[i];
		if(rTransition.m_ToPose.GetHash() == uName)
		{
			return &rTransition;
		}
	}

	return NULL;
}

const CEntity* CTaskVariedAimPose::GetTarget() const
{
	//Get the gun task.
	CTaskGun* pTask = static_cast<CTaskGun *>(FindSubTaskOfType(CTaskTypes::TASK_GUN));
	if(!pTask)
	{
		return NULL;
	}

	return pTask->GetTarget().GetEntity();
}

const CPed* CTaskVariedAimPose::GetTargetPed() const
{
	//Ensure the entity is valid.
	const CEntity* pEntity = GetTarget();
	if(!pEntity)
	{
		return NULL;
	}

	//Ensure the entity is a ped.
	if(!pEntity->GetIsTypePed())
	{
		return NULL;
	}

	const CPed* pPed = static_cast<const CPed *>(pEntity);
	return pPed;
}

bool CTaskVariedAimPose::HasPositionToMoveAwayFrom() const
{
	return (!IsCloseAll(m_vPositionToMoveAwayFrom, Vec3V(V_ZERO), ScalarV(SMALL_FLOAT)));
}

bool CTaskVariedAimPose::IsInDefaultStandingPose() const
{
	//Ensure the pose is valid.
	if(!m_pPose)
	{
		return false;
	}

	//Ensure the pose is the default standing pose.
	if(m_pPose->m_Name != sm_Tunables.m_DefaultStandingPose)
	{
		return false;
	}

	return true;
}

bool CTaskVariedAimPose::IsOffsetLargeEnoughToMatter(Vec3V_In vAnimOffset) const
{
	//Ensure the magnitude is significant.
	ScalarV scAnimOffsetMagnitudeSq = MagSquared(vAnimOffset);
	ScalarV scMinAnimOffsetMagnitudeSq = ScalarVFromF32(square(sm_Tunables.m_MinAnimOffsetMagnitude));
	if(IsLessThanAll(scAnimOffsetMagnitudeSq, scMinAnimOffsetMagnitudeSq))
	{
		return false;
	}
	
	return true;
}

bool CTaskVariedAimPose::IsOffsetOutsideAttackWindow(Vec3V_ConstRef vAnimOffset) const
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Generate the position.
	Vec3V vPos = pPed->GetTransform().GetPosition();

	//Add the offset.
	vPos += vAnimOffset;

	//Check if the position is outside the attack window.
	return (CTaskMoveWithinAttackWindow::IsPositionOutsideAttackWindow(pPed, GetTargetPed(), vPos, false));
}

bool CTaskVariedAimPose::IsOffsetOutsideDefensiveArea(Vec3V_ConstRef vAnimOffset) const
{
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Ensure the defensive area is valid.
	CDefensiveArea* pArea = pPed->GetPedIntelligence()->GetDefensiveArea();
	if(!pArea)
	{
		return false;
	}
	
	//Ensure the defensive area is active.
	if(!pArea->IsActive())
	{
		return false;
	}
	
	//Generate the position.
	Vec3V vPos = pPed->GetTransform().GetPosition();
	
	//Add the offset.
	vPos += vAnimOffset;
	
	//Check if the defensive area contains the point.
	return (!pArea->IsPointInDefensiveArea(VEC3V_TO_VECTOR3(vPos)));
}

bool CTaskVariedAimPose::IsOffsetTowardsNearbyPed(Vec3V_ConstRef vAnimOffset) const
{
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Generate the anim offset normal.
	Vec3V vAnimOffsetN = vAnimOffset;
	vAnimOffsetN.SetZ(ScalarV(V_ZERO));
	vAnimOffsetN = NormalizeSafe(vAnimOffsetN, Vec3V(V_X_AXIS_WZERO));

	//Generate the position.
	Vec3V vPos = pPed->GetTransform().GetPosition();
	
	//Generate the offset position.
	Vec3V vOffsetPos = vPos + vAnimOffset;

	//Check the nearby peds to make sure this ped is not moving towards one.
	CEntityScannerIterator entityList = pPed->GetPedIntelligence()->GetNearbyPeds();
	for(CEntity* pEntity = entityList.GetFirst(); pEntity; pEntity = entityList.GetNext())
	{
		//Grab the nearby ped position.
		Vec3V vNearbyPos = pEntity->GetTransform().GetPosition();
		
		//Generate the offset between the nearby ped and this ped.
		Vec3V vNearbyOffset = vNearbyPos - vPos;

		//Reject peds outside of a small horizontal range.
		{
			//Generate the horizontal nearby offset.
			Vec3V vNearbyOffsetH = vNearbyOffset;
			vNearbyOffsetH.SetZ(ScalarV(V_ZERO));

			//Compare the distances.
			ScalarV scMaxDistSq = ScalarVFromF32(sm_Tunables.m_AvoidNearbyPedHorizontal * sm_Tunables.m_AvoidNearbyPedHorizontal);
			if(IsGreaterThanAll(MagSquared(vNearbyOffsetH), scMaxDistSq))
			{
				continue;
			}
		}

		//Reject peds outside of a small vertical range.
		{
			//Compare the distances.
			ScalarV scMaxDist = ScalarVFromF32(sm_Tunables.m_AvoidNearbyPedVertical);
			if(IsGreaterThanAll(Abs(vNearbyOffset.GetZ()), scMaxDist))
			{
				continue;
			}
		}

		//Reject peds if the current ped is not moving towards them.
		{
			//Generate the nearby offset normal.
			Vec3V vNearbyOffsetN = vNearbyOffset;
			vNearbyOffsetN.SetZ(ScalarV(V_ZERO));
			vNearbyOffsetN = NormalizeSafe(vNearbyOffsetN, Vec3V(V_X_AXIS_WZERO));

			//Check if the ped is moving towards the nearby ped.
			ScalarV scDotThreshold = ScalarVFromF32(sm_Tunables.m_AvoidNearbyPedDotThreshold);
			if(IsLessThanAll(Dot(vAnimOffsetN, vNearbyOffsetN), scDotThreshold))
			{
				continue;
			}
		}
		
		//Reject peds if the offset will take the current ped farther away.
		{
			ScalarV scDistSq = DistSquared(vPos, vNearbyPos);
			ScalarV scOffsetDistSq = DistSquared(vOffsetPos, vNearbyPos);
			if(IsGreaterThanAll(scOffsetDistSq, scDistSq))
			{
				continue;
			}
		}
		
		return true;
	}
	
	return false;
}

bool CTaskVariedAimPose::IsOffsetTowardsPosition(Vec3V_ConstRef vAnimOffset, Vec3V_In vPos) const
{
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Generate the position.
	Vec3V vPedPos = pPed->GetTransform().GetPosition();

	//Generate the position offset.
	Vec3V vOffset = vPos - vPedPos;
	
	//Check if the ped is moving away from the position.
	ScalarV scDot = Dot(vAnimOffset, vOffset);
	if(IsLessThanAll(scDot, ScalarV(V_ZERO)))
	{
		return false;
	}
	
	return true;
}

bool CTaskVariedAimPose::IsOffsetTowardsTarget(Vec3V_ConstRef vAnimOffset) const
{
	//Grab the target.
	const CEntity* pTarget = GetTarget();
	if(!pTarget)
	{
		return false;
	}
	
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Generate the position.
	Vec3V vPos = pPed->GetTransform().GetPosition();

	//Generate the offset position.
	Vec3V vOffsetPos = vPos + vAnimOffset;
	
	//Generate the target position.
	Vec3V vTargetPos = pTarget->GetTransform().GetPosition();
	
	//Check if the offset is outside of the target radius.
	ScalarV scOffsetDistSq = DistSquared(vOffsetPos, vTargetPos);
	ScalarV scTargetRadiusSq = ScalarVFromF32(rage::square(sm_Tunables.m_TargetRadius));
	if(IsGreaterThanAll(scOffsetDistSq, scTargetRadiusSq))
	{
		return false;
	}
	
	//Check if the ped is moving away from the target.
	ScalarV scDistSq = DistSquared(vPos, vTargetPos);
	if(IsGreaterThanAll(scOffsetDistSq, scDistSq))
	{
		return false;
	}
	
	return true;
}

void CTaskVariedAimPose::MoveAwayFromPosition(Vec3V_In vPosition)
{
	//Set the position to move away from.
	m_vPositionToMoveAwayFrom = vPosition;
}

void CTaskVariedAimPose::OnGunAimedAt(const CEventGunAimedAt& rEvent)
{
	//Note that we were threatened.
	OnThreatened();
	
	//React to gun aimed at.
	ReactToGunAimedAt(rEvent);
}

void CTaskVariedAimPose::OnGunShotBulletImpact(const CEventGunShotBulletImpact& rEvent)
{
	//Note that we were threatened.
	OnThreatened();
	
	//React to gun shot bullet impact.
	ReactToGunShotBulletImpact(rEvent);
}

void CTaskVariedAimPose::OnGunShotWhizzedBy(const CEventGunShotWhizzedBy& rEvent)
{
	//Note that we were threatened.
	OnThreatened();
	
	//React to gun shot whizzed by.
	ReactToGunShotWhizzedBy(rEvent);
}

void CTaskVariedAimPose::OnShoutBlockingLos(const CEventShoutBlockingLos& rEvent)
{
	//Someone has notified us that we are blocking their line of sight.
	
	//Ensure the shouter is valid.
	const CPed* pShouter = rEvent.GetPedShouting();
	if(!pShouter)
	{
		return;
	}
	
	//Ensure the shouter is not in a vehicle.
	//If they are, we don't care about them.
	if(pShouter->GetIsInVehicle())
	{
		return;
	}
	
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Grab the ped position.
	Vec3V vPedPosition = pPed->GetTransform().GetPosition();
	
	//Grab the shouter position.
	Vec3V vShouterPosition = pShouter->GetTransform().GetPosition();
	
	//Ensure the distance has not exceeded the threshold.
	ScalarV scDistSq = DistSquared(vPedPosition, vShouterPosition);
	ScalarV scMaxDistSq = ScalarVFromF32(square(sm_Tunables.m_MaxDistanceToCareAboutBlockingLineOfSight));
	if(IsGreaterThanAll(scDistSq, scMaxDistSq))
	{
		return;
	}
	
	//Note that we are blocking someone's line of sight.
	m_uRunningFlags.SetFlag(RF_IsBlockingLineOfSight);
}

void CTaskVariedAimPose::OnThreatened()
{
	//Clear the time since we were last threatened.
	m_fTimeSinceLastThreatened = 0.0f;
}

bool CTaskVariedAimPose::PlayTransition(bool bCrouching)
{
	//Build up a transition.
	AimPose::Transition transition;
	transition.m_ToPose = bCrouching ? sm_Tunables.m_DefaultCrouchingPose : sm_Tunables.m_DefaultStandingPose;
	
	return PlayTransition(transition);
}

bool CTaskVariedAimPose::PlayTransition(const AimPose::Transition& rTransition)
{
	//Grab the ped.
	CPed* pPed = GetPed();
	
	//Grab the motion data.
	CPedMotionData* pMotionData = pPed->GetMotionData();
	
	//Set the aim pose transition.
	CPedMotionData::AimPoseTransition& rAimPoseTransition = pMotionData->GetAimPoseTransition();
	rAimPoseTransition.SetClipSetId(rTransition.m_ClipSetId);
	rAimPoseTransition.SetClipId(rTransition.m_ClipId);
	
	//Calculate the rate.
	float fRate = sm_Tunables.m_Rate * rTransition.m_Rate;
	rAimPoseTransition.SetRate(fRate);
	
	//Find the pose.
	s32 iAimPoseIdx = FindAimPoseIdx(rTransition.m_ToPose.GetHash());
	if(!taskVerifyf(iAimPoseIdx >= 0, "The aim pose idx is invalid: %s.", rTransition.m_ToPose.GetCStr()))
	{
		return false;
	}

	const AimPose* pPose = GetAimPoseAt(iAimPoseIdx);
	if(!taskVerifyf(pPose, "The aim pose is invalid at index: %d (%s).", iAimPoseIdx, rTransition.m_ToPose.GetCStr()))
	{
		return false;
	}

	if (pPed->IsNetworkClone())
	{
		CTask* pMotionTask = pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_AIMING);
		if (pMotionTask && pMotionTask->GetState() == CTaskMotionAiming::State_PlayTransition)
		{
			AI_LOG_WITH_ARGS("Ped %s cannot start a new transition whilst playing a transition\n", AILogging::GetDynamicEntityNameSafe(pPed));
			return false;
		}
	}
	
	//Grab the stationary aim pose.
	CPedMotionData::StationaryAimPose& rStationaryAimPose = pMotionData->GetStationaryAimPose();
	
	//Check if the pose is stationary.
	if(pPose->m_IsStationary)
	{
		//Set the stationary aim pose.
		rStationaryAimPose.SetLoopClipSetId(pPose->m_LoopClipSetId);
		rStationaryAimPose.SetLoopClipId(pPose->m_LoopClipId);
	}
	else
	{
		//Clear the stationary aim pose.
		rStationaryAimPose.Clear();
	}
	
	//Check if the aiming task is running.
	//It should be, in most cases.
	CTaskMotionAiming* pTask = static_cast<CTaskMotionAiming *>(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_AIMING));
	if(pTask)
	{
		//Restart the lower body aiming network.
		//This will allow the task to play the transition.
		pTask->RestartLowerBodyAimNetwork();
	}
	
	return true;
}

void CTaskVariedAimPose::ProcessCounters()
{
	//Clear the clips checked this frame.
	m_iClipsCheckedThisFrame = 0;
}

void CTaskVariedAimPose::ProcessFlags()
{
	//Consider our line of sight blocked, to begin with.
	bool bIsLineOfSightBlocked = true;
	
	//Check if the target is valid.
	const CEntity* pTarget = GetTarget();
	if(pTarget)
	{
		//Check if the targeting is valid.
		CPedTargetting* pPedTargetting = GetPed()->GetPedIntelligence()->GetTargetting(true);
		if(pPedTargetting)
		{
			//Grab the line of sight status.
			LosStatus losStatus = pPedTargetting->GetLosStatus(pTarget, false);
			
			//Check if the los is clear.
			bIsLineOfSightBlocked = (losStatus != Los_clear);
		}
	}
	
	//Change the flag.
	m_uRunningFlags.ChangeFlag(RF_IsLineOfSightBlocked, bIsLineOfSightBlocked);
}

void CTaskVariedAimPose::ProcessHeading()
{
	//Check if the ped should face the target.
	if(ShouldFaceTarget())
	{
		//Grab the target.
		const CEntity* pTarget = GetTarget();
		if(pTarget)
		{
			//Face the target.
			GetPed()->SetDesiredHeading(VEC3V_TO_VECTOR3(pTarget->GetTransform().GetPosition()));
		}
	}
}

void CTaskVariedAimPose::ProcessTimers()
{
	//Grab the time step.
	float fTimeStep = GetTimeStep();
	
	//Update the timers.
	m_fTimeSinceLastReaction					+= fTimeStep;
	m_fTimeSinceLastThreatened					+= fTimeStep;
	m_fTimeSinceLastReactionCheckForGunAimedAt	+= fTimeStep;
}

void CTaskVariedAimPose::React(Vec3V_In vPosition)
{
	//Assert that we can react.
	taskAssertf(CanReact(), "The ped is unable to react.");
	
	//Clear the time since the last reaction.
	m_fTimeSinceLastReaction = 0.0f;
	
	//Add the reaction.
	m_Reaction.m_bValid = true;
	m_Reaction.m_vPos = vPosition;
}

void CTaskVariedAimPose::ReactToGunAimedAt(const CEventGunAimedAt& rEvent)
{
	//Ensure the time between reactions to gun aimed at has expired.
	float fMinTimeBetweenReactionChecksForGunAimedAt = sm_Tunables.m_MinTimeBetweenReactionChecksForGunAimedAt;
	if(m_fTimeSinceLastReactionCheckForGunAimedAt < fMinTimeBetweenReactionChecksForGunAimedAt)
	{
		return;
	}
	
	//Ensure the ped is valid.
	const CPed* pPed = GetPed();
	if(!pPed)
	{
		return;
	}
	
	//Ensure the source entity is valid.
	const CEntity* pSourceEntity = rEvent.GetSourceEntity();
	if(!pSourceEntity)
	{
		return;
	}
	
	//Ensure the source entity is a ped.
	if(!pSourceEntity->GetIsTypePed())
	{
		return;
	}
	
	//Ensure we can react.
	if(!CanReact())
	{
		return;
	}
	
	//Clear the time since the last reaction check.
	m_fTimeSinceLastReactionCheckForGunAimedAt = 0.0f;
	
	//Check if we should react.
	float fChancesToReactForGunAimedAt = sm_Tunables.m_ChancesToReactForGunAimedAt;
	if(fChancesToReactForGunAimedAt == 0.0f)
	{
		return;
	}
	else
	{
		//Evaluate the odds to react.
		float fRandom = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
		if(fRandom > fChancesToReactForGunAimedAt)
		{
			return;
		}
	}
	
	//Grab the source ped.
	const CPed* pSourcePed = static_cast<const CPed *>(pSourceEntity);

	//Ensure the weapon manager is valid.
	const CPedWeaponManager* pWeaponManager = pSourcePed->GetWeaponManager();
	if(!pWeaponManager)
	{
		return;
	}

	//Ensure the weapon is valid.
	const CWeapon* pWeapon = pWeaponManager->GetEquippedWeapon();
	if(!pWeapon)
	{
		return;
	}
	
	//Grab the ped position.
	Vec3V vPedPosition = pPed->GetTransform().GetPosition();
	
	//Grab the source position.
	Vec3V vSourcePosition = pSourcePed->GetTransform().GetPosition();
	
	//Generate a vector from the source to the ped.
	Vec3V vSourceToPed = Subtract(vPedPosition, vSourcePosition);
	
	//Calculate the aim vector for the source ped.
	Vec3V vStart;
	Vec3V vEnd;
	Vec3V vTarget;
	CUpperBodyAimPitchHelper::CalculateAimVector(pSourcePed, RC_VECTOR3(vStart), RC_VECTOR3(vEnd), RC_VECTOR3(vTarget));
	Vec3V vSourceAim = Subtract(vEnd, vStart);
	
	//Calculate the projection of the source to ped vector on to the aim vector.
	Vec3V vSourceAimDirection = NormalizeFastSafe(vSourceAim, Vec3V(V_ZERO));
	Vec3V vProjection = Scale(Dot(vSourceToPed, vSourceAimDirection), vSourceAimDirection);
	
	//Calculate the position.
	Vec3V vPosition = Add(vSourcePosition, vProjection);
	React(vPosition);
}

void CTaskVariedAimPose::ReactToGunShotBulletImpact(const CEventGunShotBulletImpact& rEvent)
{
	//For the moment, use the same logic as whizzed by events.
	ReactToGunShotWhizzedBy(rEvent);
}

void CTaskVariedAimPose::ReactToGunShotWhizzedBy(const CEventGunShotWhizzedBy& rEvent)
{
	//Ensure the ped is valid.
	const CPed* pPed = GetPed();
	if(!pPed)
	{
		return;
	}
	
	//Ensure the bullet is in range.
	Vec3V vPosition;
	if(!rEvent.IsBulletInRange(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), vPosition))
	{
		return;
	}
	
	//Ensure we can react.
	if(!CanReact())
	{
		return;
	}
	
	//Ensure bullet reactions are not disabled.
	if(pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_DisableBulletReactions))
	{
		return;
	}
	
	//React to the event.
	React(vPosition);
}

void CTaskVariedAimPose::SetStateAndKeepSubTask(s32 iState)
{
	//Keep the sub-task across the transition.
	SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
	
	//Set the state.
	SetState(iState);
}

bool CTaskVariedAimPose::ShouldChooseNewPose() const
{
	//Ensure a new pose can be chosen.
	if(!CanChooseNewPose())
	{
		return false;
	}
	
	//Check if a new pose should be chosen due to blocking someone else's line of sight.
	if(ShouldChooseNewPoseDueToBlockingLineOfSight())
	{
		return true;
	}
	
	//Check if the ped should choose a new pose due to a blocked line of sight.
	if(ShouldChooseNewPoseDueToBlockedLineOfSight())
	{
		return true;
	}
	
	//Check if a new pose should be chosen due to moving away from a position.
	if(ShouldChooseNewPoseDueToMovingAwayFromPosition())
	{
		return true;
	}
	
	//Check if the ped should choose a new pose due to a reaction.
	if(ShouldChooseNewPoseDueToReaction())
	{
		return true;
	}
	
	//Check if the ped should choose a new pose due to time.
	if(ShouldChooseNewPoseDueToTime())
	{
		return true;
	}
	
	return false;
}

bool CTaskVariedAimPose::ShouldChooseNewPoseDueToBlockedLineOfSight() const
{
	//Ensure our line of sight is blocked.
	if(!m_uRunningFlags.IsFlagSet(RF_IsLineOfSightBlocked))
	{
		return false;
	}
	
	//Ensure we haven't failed to fix our line of sight.
	if(m_uRunningFlags.IsFlagSet(RF_FailedToFixLineOfSight))
	{
		return false;
	}
	
	return true;
}

bool CTaskVariedAimPose::ShouldChooseNewPoseDueToBlockingLineOfSight() const
{
	//Ensure we are blocking line of sight.
	if(!m_uRunningFlags.IsFlagSet(RF_IsBlockingLineOfSight))
	{
		return false;
	}
	
	return true;
}

bool CTaskVariedAimPose::ShouldChooseNewPoseDueToMovingAwayFromPosition() const
{
	return HasPositionToMoveAwayFrom();
}

bool CTaskVariedAimPose::ShouldChooseNewPoseDueToReaction() const
{
	return (m_Reaction.m_bValid);
}

bool CTaskVariedAimPose::ShouldChooseNewPoseDueToTime() const
{
	//Ensure the target is valid.
	const CEntity* pTarget = GetTarget();
	if(!pTarget)
	{
		return false;
	}
	
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Grab the ped position.
	Vec3V vPedPosition = pPed->GetTransform().GetPosition();
	
	//Grab the target position.
	Vec3V vTargetPosition = pTarget->GetTransform().GetPosition();
	
	//Calculate the distance.
	float fDistSq = DistSquared(vPedPosition, vTargetPosition).Getf();
	
	//Clamp the distance.
	float fDistanceForMinTimeBeforeNewPose = sm_Tunables.m_DistanceForMinTimeBeforeNewPose;
	float fDistanceForMinTimeBeforeNewPoseSq = square(fDistanceForMinTimeBeforeNewPose);
	float fDistanceForMaxTimeBeforeNewPose = sm_Tunables.m_DistanceForMaxTimeBeforeNewPose;
	float fDistanceForMaxTimeBeforeNewPoseSq = square(fDistanceForMaxTimeBeforeNewPose);
	fDistSq = Clamp(fDistSq, fDistanceForMinTimeBeforeNewPoseSq, fDistanceForMaxTimeBeforeNewPoseSq);
	
	//Normalize the value.
	float fLerp = (fDistSq - fDistanceForMinTimeBeforeNewPoseSq) / (fDistanceForMaxTimeBeforeNewPoseSq - fDistanceForMinTimeBeforeNewPoseSq);
	
	//Ensure the ped has spent the minimum amount of time in this state.
	float fTimeInState = GetTimeInState();
	float fMinTimeBeforeNewPose = sm_Tunables.m_MinTimeBeforeNewPose;
	float fMaxTimeBeforeNewPose = sm_Tunables.m_MaxTimeBeforeNewPose;
	float fTimeBeforeNewPose = Lerp(fLerp, fMinTimeBeforeNewPose, fMaxTimeBeforeNewPose);
	if(fTimeInState < fTimeBeforeNewPose)
	{
		return false;
	}
	
	return true;
}

bool CTaskVariedAimPose::ShouldFaceTarget() const
{
	return (GetState() == State_PlayTransition);
}

bool CTaskVariedAimPose::ShouldUseUrgentTransitions() const
{
	//Check if we should use urgent trnasitions due to attributes.
	if(ShouldUseUrgentTransitionsDueToAttributes())
	{
		return true;
	}

	//Check if we should use urgent transitions due to threatened.
	if(ShouldUseUrgentTransitionsDueToThreatened())
	{
		return true;
	}
	
	//Check if we should use urgent transitions due to distance.
	if(ShouldUseUrgentTransitionsDueToDistance())
	{
		return true;
	}
	
	return false;
}

bool CTaskVariedAimPose::ShouldUseUrgentTransitionsDueToAttributes() const
{
	//Check if the ped is a gang ped.
	if(GetPed()->IsGangPed())
	{
		return true;
	}

	return false;
}

bool CTaskVariedAimPose::ShouldUseUrgentTransitionsDueToDistance() const
{
	//Ensure the target is valid.
	const CEntity* pTarget = GetTarget();
	if(!pTarget)
	{
		return false;
	}
	
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Grab the ped position.
	Vec3V vPedPosition = pPed->GetTransform().GetPosition();
	
	//Grab the target position.
	Vec3V vTargetPosition = pTarget->GetTransform().GetPosition();
	
	//Ensure the distance is within the threshold.
	ScalarV scDistSq = DistSquared(vPedPosition, vTargetPosition);
	ScalarV scMaxDistSq = ScalarVFromF32(square(sm_Tunables.m_MaxDistanceToUseUrgentTransitions));
	if(IsGreaterThanAll(scDistSq, scMaxDistSq))
	{
		return false;
	}
	
	return true;
}

bool CTaskVariedAimPose::ShouldUseUrgentTransitionsDueToThreatened() const
{
	//Ensure the time is within the threshold.
	float fTimeToUseUrgentTransitionsWhenThreatened = sm_Tunables.m_TimeToUseUrgentTransitionsWhenThreatened;
	if(m_fTimeSinceLastThreatened > fTimeToUseUrgentTransitionsWhenThreatened)
	{
		return false;
	}
	
	return true;
}

void CTaskVariedAimPose::StartTestLineOfSight(Vec3V_ConstRef vPosition)
{
	//Assert that the LOS helper is not active.
	taskAssertf(!m_AsyncProbeHelper.IsProbeActive(), "LOS Helper is already active.");
	
	//Ensure the target is valid.
	const CEntity* pTarget = GetTarget();
	if(!pTarget)
	{
		return;
	}

	//Generate the exception list.
	const CEntity* pExceptionList[2];
	u8 uExceptionListCount = 0;
	pExceptionList[uExceptionListCount++] = pTarget;
	
	//Check if the target is a ped.
	if(pTarget->GetIsTypePed())
	{
		//Grab the ped.
		const CPed* pTargetPed = static_cast<const CPed *>(pTarget);
		
		//Add an exception for the ped's vehicle.
		pExceptionList[uExceptionListCount++] = pTargetPed->GetVehiclePedInside();
	}

	//Generate a probe for LOS.
	WorldProbe::CShapeTestProbeDesc probeData;
	probeData.SetStartAndEnd(VEC3V_TO_VECTOR3(vPosition),VEC3V_TO_VECTOR3(pTarget->GetTransform().GetPosition()));
	probeData.SetContext(WorldProbe::ELosCombatAI);
	probeData.SetExcludeEntities(pExceptionList, uExceptionListCount);
	probeData.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_OBJECT_TYPE|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_PED_TYPE);
	probeData.SetOptions(WorldProbe::LOS_IGNORE_SEE_THRU);

	//Start the LOS test.
	m_AsyncProbeHelper.StartTestLineOfSight(probeData);
}

void CTaskVariedAimPose::UpdatePoseFromNetwork()
{
	if (m_FromNetworkSerializableState.GetPoseIdx() >= 0 && 
		m_FromNetworkSerializableState.GetPoseIdx() != m_LocalSerializableState.GetPoseIdx())
	{
		ChangePose(m_FromNetworkSerializableState.GetPoseIdx());
	}
}

void CTaskVariedAimPose::UpdateTransitionFromNetwork()
{
	if (m_FromNetworkSerializableState.GetTransitionIdx() >= 0 &&
		m_FromNetworkSerializableState.GetTransitionIdx() != m_LocalSerializableState.GetTransitionIdx())
	{
		taskAssertf(m_LocalSerializableState.GetPoseIdx() != -1 &&
					m_LocalSerializableState.GetPoseIdx() == m_FromNetworkSerializableState.GetPoseIdx(),
			"CTaskVariedAimPose::UpdateTransitionFromNetwork: poses should be the same. m_LocalSerializableState.GetPoseIdx(): %d, m_FromNetworkSerializableState.GetPoseIdx(): %d", m_LocalSerializableState.GetPoseIdx(), m_FromNetworkSerializableState.GetPoseIdx());
		SetTransition(m_FromNetworkSerializableState.GetTransitionIdx());
	}
}

void CTaskVariedAimPose::UpdateTransitionClipIdFromNetwork()
{
	if (m_FromNetworkSerializableState.GetTransitionClipId() != CLIP_ID_INVALID && 
		m_FromNetworkSerializableState.GetTransitionClipId() != m_LocalSerializableState.GetTransitionClipId())
	{
		taskAssertf(m_LocalSerializableState.GetPoseIdx() != -1 && 
					m_LocalSerializableState.GetPoseIdx() == m_FromNetworkSerializableState.GetPoseIdx() && 
					m_LocalSerializableState.GetTransitionIdx() != -1 &&
					m_LocalSerializableState.GetTransitionIdx() == m_FromNetworkSerializableState.GetTransitionIdx(),
			"CTaskVariedAimPose::UpdateTransitionClipIdFromNetwork: poses and transitions should be the same.");
		SetTransitionClipId(fwMvClipId(m_FromNetworkSerializableState.GetTransitionClipId()));
	}
}

void CTaskVariedAimPose::UpdateTransitionPlayStatusFromNetwork()
{
	if (m_FromNetworkSerializableState.GetTransitionPlayStatus() != CSerializableState::STransitionPlayStatus::UNDEFINED && 
		m_FromNetworkSerializableState.GetTransitionPlayStatus() != m_LocalSerializableState.GetTransitionPlayStatus())
	{
		taskAssertf(m_LocalSerializableState.GetPoseIdx() != -1 && 
			m_LocalSerializableState.GetPoseIdx() == m_FromNetworkSerializableState.GetPoseIdx() && 
			m_LocalSerializableState.GetTransitionIdx() != -1 &&
			m_LocalSerializableState.GetTransitionIdx() == m_FromNetworkSerializableState.GetTransitionIdx() &&
			m_LocalSerializableState.GetTransitionClipId() != -1 &&
			m_LocalSerializableState.GetTransitionClipId() == m_FromNetworkSerializableState.GetTransitionClipId() &&
			m_LocalSerializableState.GetTransitionPlayStatus() == CSerializableState::STransitionPlayStatus::UNDEFINED,
			"CTaskVariedAimPose::UpdateTransitionPlayStatusFromNetwork: poses, transitions and clips should be the same and the status should be undefined.");
		SetTransitionPlayStatus(m_FromNetworkSerializableState.GetTransitionPlayStatus());
	}
}

#if !__FINAL
void CTaskVariedAimPose::Debug() const
{
#if DEBUG_DRAW
	//Ensure debug draw is enabled.
	if(!sm_Tunables.m_DebugDraw)
	{
		return;
	}
	
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Create the buffer.
	char buf[128];

	//Keep track of the line.
	int iLine = 0;

	//Calculate the text position.
	Vector3 vTextPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) + (ZAXIS * 1.5f);

	formatf(buf, "Is Line Of Sight Blocked: %s", m_uRunningFlags.IsFlagSet(RF_IsLineOfSightBlocked) ? "yes" : "no");
	grcDebugDraw::Text(vTextPosition, Color_blue, 0, iLine++ * grcDebugDraw::GetScreenSpaceTextHeight(), buf);

	formatf(buf, "Failed To Fix Line Of Sight: %s", m_uRunningFlags.IsFlagSet(RF_FailedToFixLineOfSight) ? "yes" : "no");
	grcDebugDraw::Text(vTextPosition, Color_blue, 0, iLine++ * grcDebugDraw::GetScreenSpaceTextHeight(), buf);

	formatf(buf, "Is Blocking Line Of Sight: %s", m_uRunningFlags.IsFlagSet(RF_IsBlockingLineOfSight) ? "yes" : "no");
	grcDebugDraw::Text(vTextPosition, Color_blue, 0, iLine++ * grcDebugDraw::GetScreenSpaceTextHeight(), buf);

	formatf(buf, "Moving Away From Position: %s", m_uRunningFlags.IsFlagSet(RF_MovingAwayFromPosition) ? "yes" : "no");
	grcDebugDraw::Text(vTextPosition, Color_blue, 0, iLine++ * grcDebugDraw::GetScreenSpaceTextHeight(), buf);
	
	formatf(buf, "Has Position To Move Away From: %s", HasPositionToMoveAwayFrom() ? "yes" : "no");
	grcDebugDraw::Text(vTextPosition, Color_blue, 0, iLine++ * grcDebugDraw::GetScreenSpaceTextHeight(), buf);
#endif

	//Call the base class version.
	CTask::Debug();
}
#endif // !__FINAL

#if !__FINAL
const char * CTaskVariedAimPose::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Start&&iState<=State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Shoot",
		"State_ChooseTransition",
		"State_CheckNextTransition",
		"State_StreamClipSet",
		"State_ChooseClip",
		"State_CheckNextClip",
		"State_CheckLineOfSight",
		"State_CheckNavMesh",
		"State_CheckProbe",
		"State_PlayTransition",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL


////////////////////////////////////////////////////////////////////////////////
// CLASS: CClonedVariedAimPoseInfo
// TaskInfo class for CTaskVariedAimPose
////////////////////////////////////////////////////////////////////////////////

CClonedVariedAimPoseInfo::CClonedVariedAimPoseInfo(s32 const iState, CTaskVariedAimPose::CSerializableState const& rSerialisablePoseTransitionInfo)
{
	SetStatusFromMainTaskState(iState);	

	m_PoseTransitionInfo = rSerialisablePoseTransitionInfo;
}

////////////////////////////////////////////////////////////////////////////////

CClonedVariedAimPoseInfo::CClonedVariedAimPoseInfo() 
{}

////////////////////////////////////////////////////////////////////////////////

CClonedVariedAimPoseInfo::~CClonedVariedAimPoseInfo()
{}

////////////////////////////////////////////////////////////////////////////////

CTaskFSMClone* CClonedVariedAimPoseInfo::CreateCloneFSMTask()
{
	return rage_new CTaskVariedAimPose(NULL);
}

////////////////////////////////////////////////////////////////////////////////

CTask* CClonedVariedAimPoseInfo::CreateLocalTask(fwEntity* UNUSED_PARAM( pEntity ) )
{
	return rage_new CTaskVariedAimPose(NULL);
}
