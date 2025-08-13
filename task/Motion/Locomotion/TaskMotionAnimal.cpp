#include "Task/Motion/Locomotion/TaskMotionAnimal.h"

#if ENABLE_HORSE

// rage includes

// game includes
#include "animation/MovePed.h"
#include "Task/Motion/Locomotion/TaskFishLocomotion.h"
#include "Task/Motion/Locomotion/TaskHorseLocomotion.h"
#include "Task/Motion/Locomotion/TaskInWater.h"
#include "Task/Motion/Locomotion/TaskQuadLocomotion.h"
#include "Task/System/MotionTaskData.h"
#include "Peds\Horse\horseComponent.h"

AI_OPTIMISATIONS() 

const fwMvNetworkId CTaskMotionAnimal::ms_OnFootNetworkId("OnFootNetwork",0xe40b89c2);
const fwMvRequestId CTaskMotionAnimal::ms_OnFootId("OnFoot",0xf5a638b9);
const fwMvBooleanId CTaskMotionAnimal::ms_OnEnterOnFootId("OnEnter_OnFoot",0xd850b358);

const fwMvClipSetVarId CTaskMotionAnimal::ms_SwimmingClipSetId("Swimming",0x5da2a411);
const fwMvNetworkId CTaskMotionAnimal::ms_SwimmingNetworkId("SwimmingNetwork",0xc419414c);
const fwMvRequestId CTaskMotionAnimal::ms_SwimmingId("Swimming",0x5da2a411);
const fwMvBooleanId CTaskMotionAnimal::ms_OnEnterSwimmingId("OnEnter_Swimming",0x912b6ec2);

const fwMvRequestId CTaskMotionAnimal::ms_SwimToRunId("SwimToRun",0x26cff917);
const fwMvBooleanId CTaskMotionAnimal::ms_OnEnterSwimToRunId("OnEnter_SwimToRun",0x24435b26);
const fwMvFloatId   CTaskMotionAnimal::ms_SwimToRunPhaseId("SwimToRunPhase",0xf8309fcf);
const fwMvBooleanId CTaskMotionAnimal::ms_TransitionClipFinishedId("TransitionAnimFinished",0x96e8ae49);
const fwMvClipId	CTaskMotionAnimal::ms_SwimToRunClipId("swim_to_run",0x8967494b);

CTaskMotionAnimal::CTaskMotionAnimal()
{
	SetInternalTaskType(CTaskTypes::TASK_MOTION_ANIMAL);
};

CTaskMotionAnimal::~CTaskMotionAnimal()
{
}

// Need a way to properly query the locomotion clips here
void CTaskMotionAnimal::GetMoveSpeeds(CMoveBlendRatioSpeeds &speeds)
{
	if(GetSubTask())
	{
		taskAssert(dynamic_cast<CTaskMotionBase*>(GetSubTask()));
		static_cast<CTaskMotionBase*>(GetSubTask())->GetMoveSpeeds(speeds);
	}
	else
	{
		speeds.SetWalkSpeed(0.0f);
		speeds.SetRunSpeed(0.0f);
		speeds.SetSprintSpeed(0.0f);
	}
}

//***********************************************************************************************
// This function returns whether the ped is allowed to fly through the air in its current state
bool CTaskMotionAnimal::CanFlyThroughAir()
{
	if (GetSubTask())
	{
		CTaskMotionBase* pSubTask = static_cast<CTaskMotionBase*>(GetSubTask());
		return pSubTask->CanFlyThroughAir();
	}
	return CTaskMotionBase::CanFlyThroughAir();
}

//*************************************************************************************
// Whether the ped should be forced to stick to the floor based upon the movement mode
bool CTaskMotionAnimal::ShouldStickToFloor()
{
	if (GetSubTask())
	{
		CTaskMotionBase* pSubTask = static_cast<CTaskMotionBase*>(GetSubTask());
		return pSubTask->ShouldStickToFloor();
	}

	return CTaskMotionBase::ShouldStickToFloor();
}

//*********************************************************************************
// Returns whether the ped is currently moving based upon internal movement forces
bool CTaskMotionAnimal::IsInMotion(const CPed * pPed) const
{
	if (GetSubTask())
	{
		const CTaskMotionBase* pSubTask = static_cast<const CTaskMotionBase*>(GetSubTask());
		return pSubTask->IsInMotion(pPed);
	}

	return false;
}

void CTaskMotionAnimal::GetNMBlendOutClip(fwMvClipSetId& outClipSetId, fwMvClipId& outClipId)
{
	if (GetSubTask())
	{
		static_cast<CTaskMotionBase*>(GetSubTask())->GetNMBlendOutClip(outClipSetId, outClipId);
	}
	else
	{
		outClipSetId = m_defaultOnFootClipSet;
		outClipId = CLIP_IDLE;
	}
}

bool CTaskMotionAnimal::IsInMotionState(CPedMotionStates::eMotionState state) const
{
	if (GetSubTask())
	{
		const CTaskMotionBase* pSubTask = static_cast<const CTaskMotionBase*>(GetSubTask());
		return pSubTask->IsInMotionState(state);
	}

	return false;
}

CTask::FSM_Return CTaskMotionAnimal::ProcessPostFSM()
{
	return FSM_Continue;
}

CTask::FSM_Return CTaskMotionAnimal::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		// Entry point
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();
		FSM_State(State_OnFoot)
			FSM_OnEnter
				OnFoot_OnEnter();
			FSM_OnUpdate
				return OnFoot_OnUpdate();
			FSM_OnExit
				OnFoot_OnExit();
		FSM_State(State_Swimming)
			FSM_OnEnter
				Swimming_OnEnter();
			FSM_OnUpdate
				return Swimming_OnUpdate();
			FSM_OnExit
				Swimming_OnExit();
		FSM_State(State_SwimToRun)
			FSM_OnEnter
				SwimToRun_OnEnter();
			FSM_OnUpdate
				return SwimToRun_OnUpdate();
		FSM_State(State_Exit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskMotionAnimal::GetStaticStateName( s32 iState )
{
	switch(iState)
	{
	case State_Start:					return "State_Start";
	case State_OnFoot:					return "State_OnFoot";
	case State_Swimming:				return "State_Swimming";
	case State_SwimToRun:				return "State_SwimToRun";
	case State_Exit:					return "State_Exit";
	default:
		Assertf(false, "CTaskMotionAnimal::GetStaticStateName : Invalid State %d", iState);
		return "Unknown";
	}
}
#endif // !__FINAL

//////////////////////////////////////////////////////////////////////////

void	CTaskMotionAnimal::CleanUp()
{
	if (m_MoveNetworkHelper.IsNetworkActive())
	{
		GetPed()->GetMovePed().ClearMotionTaskNetwork(m_MoveNetworkHelper);
	}
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskMotionAnimal::Start_OnUpdate()
{
	//stream assets and start the primary motion move network
	CPed* pPed = GetPed();
	m_MoveNetworkHelper.RequestNetworkDef( CClipNetworkMoveInfo::ms_NetworkMotionAnimal ); // TODO: Ensure this is only called once.
	if (m_MoveNetworkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_NetworkMotionAnimal))
	{
		m_MoveNetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkMotionAnimal );
		pPed->GetMovePed().SetMotionTaskNetwork( m_MoveNetworkHelper.GetNetworkPlayer(), 0.0f, CMovePed::MotionTask_TagSyncTransition );

		bool leaveDesiredMbr = pPed->GetPedResetFlag(CPED_RESET_FLAG_ForceMotionStateLeaveDesiredMBR);

		switch(GetMotionData().GetForcedMotionStateThisFrame())
		{
		case CPedMotionStates::MotionState_Idle :
			GetMotionData().SetCurrentMoveBlendRatio(MOVEBLENDRATIO_STILL);
			if (!leaveDesiredMbr) 
				GetMotionData().SetDesiredMoveBlendRatio(MOVEBLENDRATIO_STILL);
			SetDefaultMotionState();
			break;
		case CPedMotionStates::MotionState_Walk :
			GetMotionData().SetCurrentMoveBlendRatio(MOVEBLENDRATIO_WALK);
			if (!leaveDesiredMbr) 
				GetMotionData().SetDesiredMoveBlendRatio(MOVEBLENDRATIO_WALK);
			SetDefaultMotionState();
			break;
		case CPedMotionStates::MotionState_Run :
			GetMotionData().SetCurrentMoveBlendRatio(MOVEBLENDRATIO_RUN);
			if (!leaveDesiredMbr) 
				GetMotionData().SetDesiredMoveBlendRatio(MOVEBLENDRATIO_RUN);
			SetDefaultMotionState();
			break;
		case CPedMotionStates::MotionState_Sprint :
			GetMotionData().SetCurrentMoveBlendRatio(MOVEBLENDRATIO_SPRINT);
			if (!leaveDesiredMbr) 
				GetMotionData().SetDesiredMoveBlendRatio(MOVEBLENDRATIO_SPRINT);
			SetDefaultMotionState();
			break;
		default:
			SetDefaultMotionState();
		}

		if( pPed->GetPedResetFlag(CPED_RESET_FLAG_SpawnedThisFrameByAmbientPopulation) )
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_DontRenderThisFrame, true);
			pPed->SetRenderDelayFlag(CPed::PRDF_DontRenderUntilNextTaskUpdate);
			pPed->SetRenderDelayFlag(CPed::PRDF_DontRenderUntilNextAnimUpdate);
		}
		else if( pPed->PopTypeIsMission() || 
			(pPed->GetHorseComponent() && 
			pPed->GetHorseComponent()->GetRider() && 
			pPed->GetHorseComponent()->GetRider()->IsPlayer()))
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true);
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ePostCameraAnimUpdateUseZeroTimestep, true);
		}
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////
// On foot 

void CTaskMotionAnimal::OnFoot_OnEnter()
{
	CPed* pPed = GetPed();

	CTaskMotionBase* pMotionTask;
	if ( pPed->GetMotionTaskDataSet()->GetOnFootData()->m_Type == HORSE_ON_FOOT )
	{
		pMotionTask = rage_new CTaskHorseLocomotion(0, 0, 0, pPed->GetPedModelInfo()->GetMovementClipSet());
	}
	else
	{
		pMotionTask = rage_new CTaskQuadLocomotion(0, 0, 0, pPed->GetPedModelInfo()->GetMovementClipSet());
	}

	// Need to change from Swimming to OnFoot in the move network
	m_MoveNetworkHelper.SendRequest(ms_OnFootId);
	pMotionTask->GetMoveNetworkHelper()->SetupDeferredInsert(&m_MoveNetworkHelper, ms_OnEnterOnFootId, ms_OnFootNetworkId);

	SetNewTask(pMotionTask);
}

CTask::FSM_Return CTaskMotionAnimal::OnFoot_OnUpdate()
{
	// Wait for the move network to reach the OnFoot state
	if (!m_MoveNetworkHelper.IsInTargetState())
	{
		return FSM_Continue;
	}

	taskAssert( GetSubTask() && (GetSubTask()->GetTaskType()==CTaskTypes::TASK_ON_FOOT_HORSE || GetSubTask()->GetTaskType()==CTaskTypes::TASK_ON_FOOT_QUAD ) ) ;

	// Check transition to swimming if we have a waterdata definition in the motion taskdata set
	CPed* pPed = GetPed();
	const sMotionTaskData* pInWaterData = pPed->GetMotionTaskDataSet()->GetInWaterData();
	if ( pInWaterData != NULL )
	{
		if( pPed->GetIsSwimming() )
		{
			SetState(State_Swimming);
		}
	}

	return FSM_Continue;
}

void CTaskMotionAnimal::OnFoot_OnExit()
{

}

//////////////////////////////////////////////////////////////////////////
// swimming 

void	CTaskMotionAnimal::Swimming_OnEnter()
{
	// start the subtask
	CTaskMotionBase* pMotionTask;

	// If the swimming clipset has not been specified in the MotionTaskData 
	if ( GetDefaultSwimmingClipSet() == CLIP_SET_ID_INVALID )
	{
		Assertf(false, "There is no swimming clipset defined in motiontaskdata.meta for this ped model (%s). Fallback to using human swimming animations.", GetPed()->GetModelName() );
		SetDefaultSwimmingClipSet( fwMvClipSetId("move_ped_swimming") );
		SetDefaultSwimmingBaseClipSet( fwMvClipSetId("move_ped_swimming_base") );
	}

	// If we dont have a swimming baseclipset use the simpler taskfishlocomotion
	if ( GetDefaultSwimmingBaseClipSet() == CLIP_SET_ID_INVALID )
	{
		pMotionTask = rage_new CTaskFishLocomotion(0, 0, 0, GetDefaultSwimmingClipSet());
	}
	// Use the more complex task motion swimming with a bigger set of specific swimming animations
	else 
	{
		pMotionTask = rage_new CTaskMotionSwimming( CTaskMotionSwimming::State_SwimStart, false );	
	}
  
	// Need to change from OnFoot to Swimming in the move network
	m_MoveNetworkHelper.SendRequest(ms_SwimmingId);
	pMotionTask->GetMoveNetworkHelper()->SetupDeferredInsert(&m_MoveNetworkHelper, ms_OnEnterSwimmingId, ms_SwimmingNetworkId);

	SetNewTask( pMotionTask );
}

CTask::FSM_Return	CTaskMotionAnimal::Swimming_OnUpdate()
{
	// Wait for the move network to reach the Swimming state
	if (!m_MoveNetworkHelper.IsInTargetState())
	{
		return FSM_Continue;
	}

	taskAssert( GetSubTask() && (GetSubTask()->GetTaskType()==CTaskTypes::TASK_ON_FOOT_FISH || GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOTION_SWIMMING ) ) ;

	// Check transition back to on foot if we have onfoot motion data set
	CPed* pPed = GetPed();
	const sMotionTaskData* pOnFootData = pPed->GetMotionTaskDataSet()->GetOnFootData();
	if ( pOnFootData != NULL && pOnFootData->m_Type != FISH_IN_WATER )
	{
		if(!pPed->GetIsSwimming())
		{
			// Figure out if we have an animation to perform the swim to run transition
			m_SwimClipSetRequestHelper.Request( GetDefaultSwimmingClipSet() ); 
			fwClipSet* pClipSet = m_SwimClipSetRequestHelper.GetClipSet();
			bool bHasSwimToRunTransition = pClipSet ? pClipSet->GetClip( ms_SwimToRunClipId ) != NULL : false;
			
			// Swim to run if we do have that transition
			if ( bHasSwimToRunTransition )
			{
				SetState(State_SwimToRun);					
			}
			// Blend directly to onfoot if we dont
			else
			{
				SetState(State_OnFoot);
			}
		}
	}

	return FSM_Continue;
}

void	CTaskMotionAnimal::Swimming_OnExit()
{
}


//////////////////////////////////////////////////////////////////////////
// swim to run

void	CTaskMotionAnimal::SwimToRun_OnEnter()
{
	//Setup the swimming clipset for the swimtorun transitions
	m_MoveNetworkHelper.SetClipSet(GetDefaultSwimmingClipSet(), ms_SwimmingClipSetId);

	// Send request and wait on update
	m_MoveNetworkHelper.SendRequest( ms_SwimToRunId );	
	m_MoveNetworkHelper.WaitForTargetState( ms_OnEnterSwimToRunId );
}

CTask::FSM_Return	CTaskMotionAnimal::SwimToRun_OnUpdate()
{
	// Wait for the move network to sync
	if (!m_MoveNetworkHelper.IsInTargetState())
	{
		return FSM_Continue;
	}

	// 
	static dev_float MIN_BREAKOUT_PHASE = 0.25f;	
	static dev_float RUNNING_BREAKOUT_PHASE = 0.6f;	

	// Running
	CPed* pPed = GetPed();
	if ( pPed->GetMotionData()->GetCurrentMbrY() > MOVEBLENDRATIO_WALK ) 
	{
		// Change to on foot if transition clip finished
		if (m_MoveNetworkHelper.GetBoolean(ms_TransitionClipFinishedId) ) 
		{		
			SetState(State_OnFoot);
			return FSM_Continue;
		}

		// Change to onfoot after 0.6f when running
		float fSwimToRunPhase = m_MoveNetworkHelper.GetFloat( ms_SwimToRunPhaseId );
		if( fSwimToRunPhase > RUNNING_BREAKOUT_PHASE ) 
		{
			m_MoveNetworkHelper.SetBoolean(ms_TransitionClipFinishedId, true);	
			SetState(State_OnFoot);
			return FSM_Continue;	
		}

		// Change to on foot between 0.25 and 0.6 if there is a significant heading change
		float fHeadingDiff = SubtractAngleShorter(pPed->GetMotionData()->GetCurrentHeading(), pPed->GetMotionData()->GetDesiredHeading());
		static dev_float MIN_ALLOWED_HEADING_CHANGE = 0.15f;
		if( (fSwimToRunPhase > MIN_BREAKOUT_PHASE && Abs(fHeadingDiff) > MIN_ALLOWED_HEADING_CHANGE) ) //break for heading change
		{
			m_MoveNetworkHelper.SetBoolean(ms_TransitionClipFinishedId, true);	
			SetState(State_OnFoot);
			return FSM_Continue;
		} 

		return FSM_Continue;
	} 

	// If not running change to on foot just after the min breakout phase
	float fSwimToRunPhase = m_MoveNetworkHelper.GetFloat( ms_SwimToRunPhaseId );
	if ( fSwimToRunPhase > MIN_BREAKOUT_PHASE )
	{
		m_MoveNetworkHelper.SetBoolean(ms_TransitionClipFinishedId, true);							
		SetState(State_OnFoot);
		return FSM_Continue;
	}

	return FSM_Continue;
}

void CTaskMotionAnimal::SetDefaultMotionState()
{
	CPed* pPed = GetPed();

	// If I have an onfoot data and it is not FISH_IN_WATER and I am not swimming
	const sMotionTaskData* pOnFootData = pPed->GetMotionTaskDataSet()->GetOnFootData();
	if ( pOnFootData && pOnFootData->m_Type != FISH_IN_WATER && !pPed->GetIsSwimming() )
	{
		SetState( State_OnFoot );
		m_MoveNetworkHelper.ForceStateChange(ms_OnFootId);
	}
	// I am swimming but no waterdata defined... so keep going on foot... 
	// I will probably just drown or move in the sand surface
	else if ( !pPed->GetMotionTaskDataSet()->GetInWaterData() ) 
	{
		SetState( State_OnFoot );
		m_MoveNetworkHelper.ForceStateChange(ms_OnFootId);
	}
	// Go to swimming
	else
	{
		SetState( State_Swimming );
		m_MoveNetworkHelper.ForceStateChange(ms_SwimmingId);
	}	
}

#endif // ENABLE_HORSE