#include "TaskBasic.h"

// Rage Headers
#include "crskeleton/skeleton.h"

// Framework headers
#include "fwanimation/animmanager.h"
#include "grcore/debugdraw.h"
#include "fwmaths/Angle.h"
#include "fwmaths/Random.h"

// Game Headers
#include "Animation/AnimBones.h"
#include "Animation/AnimManager.h"
#include "animation/AnimDefines.h"
#include "Animation/MovePed.h"
#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/aim/FirstPersonAimCamera.h"
#include "cranimation/framefilters.h"
#include "Control/TrafficLights.h"
#include "Debug/DebugScene.h"
#include "Event/EventDamage.h"
#include "Event/EventGroup.h"
#include "Event/Events.h"
#include "event/ShockingEvents.h"
#include "Game/ModelIndices.h"
#include "game/zones.h"
#include "IK/IkManager.h"
#include "IK/IkManagerSolverTypes.h"
#include "Ik/solvers/ArmIkSolver.h"
#include "modelinfo/vehiclemodelinfo.h"
#include "Objects/Door.h"
#include "Objects/DoorTuning.h"
#include "PedGroup/PedGroup.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/PedIKSettings.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedMoveBlend/PedMoveBlendManager.h"
#include "Peds/PedPhoneComponent.h"
#include "Peds/PedPlacement.h"
#include "Peds/Ped.h"
#include "phBound/Bound.h"
#include "phbound/boundcomposite.h"
#include "Physics/Constrainthalfspace.h"
#include "Physics/GtaInst.h"
#include "Physics/Physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "Scene/2dEffect.h"
#include "Scene/Entity.h"
#include "fwscene/search/SearchVolumes.h"
#include "script/Handlers/GameScriptEntity.h"
#include "Script/Script.h"
#include "Streaming/Streaming.h"
#include "Task/Default/TaskAmbient.h"
#include "Task/Animation/TaskAnims.h"
#include "Task/Animation/TaskScriptedAnimation.h"
#include "Task/Response/TaskFlee.h"
#include "Task/Response/TaskGangs.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/General/TaskSecondary.h"
#include "Task/General/Phone/TaskMobilePhone.h"
#include "Task/System/TaskTypes.h"
#include "Task/Combat/TaskCombat.h"
#include "Task/Combat/TaskDamageDeath.h"
#include "Task/Combat/Cover/TaskSeekCover.h"
#include "task/Combat/TaskThreatResponse.h"
#include "Task/Movement/TaskCollisionResponse.h"
#include "Task/Movement/TaskGoto.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Movement/TaskSlideToCoord.h"
#include "Task/Movement/TaskSeekEntity.h"
#include "Task/Physics/TaskNM.h"
#include "Task/Scenario/TaskScenario.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskCarUtils.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskRideHorse.h"
#include "Vector/colors.h"
#include "Vehicles/Wheel.h"
#include "Vfx/Misc/Fire.h"
#include "vehicleAi/driverpersonality.h"
#include "frontend/MobilePhone.h"
#include "script/Handlers/GameScriptResources.h"
#include "Math/angmath.h"

AI_OPTIMISATIONS()

const int CTaskDoNothing::ms_iDefaultDuration = 20000;

CTaskDoNothing::CTaskDoNothing(const int iDurationMs, const int iNumFramesToRun, bool makeMountStandStill, bool bEnableTimeslicing)
	: m_iDurationMs(iDurationMs)
	, m_iNumFramesToRun(iNumFramesToRun)
	, m_bMakeMountStandStill(makeMountStandStill)
	, m_bEnableTimeslicing(bEnableTimeslicing)
{
	if(m_iDurationMs >= 0)
	{
		m_Timer.Set(fwTimer::GetTimeInMilliseconds(), m_iDurationMs);
	}
	m_iNumFramesRunSoFar = 0;
	SetInternalTaskType(CTaskTypes::TASK_DO_NOTHING);
}

CTaskDoNothing::~CTaskDoNothing()
{

}

CTask::FSM_Return CTaskDoNothing::ProcessPreFSM()
{
	GetPed()->SetPedResetFlag( CPED_RESET_FLAG_GestureAnimsAllowed, true );

	GetPed()->GetPedIntelligence()->SetCheckShockFlag(true);

	return FSM_Continue;
}

CTask::FSM_Return CTaskDoNothing::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin

		FSM_State(State_Initial)
			FSM_OnEnter
				return StateInitial_OnEnter(pPed);
			FSM_OnUpdate
				return StateInitial_OnUpdate(pPed);

		FSM_State(State_StandStillForever)
			FSM_OnEnter
				return StateStandStillForever_OnEnter(pPed);
			FSM_OnUpdate
				return StateStandStillForever_OnUpdate(pPed);

		FSM_State(State_StandStillTimed)
			FSM_OnEnter
				return StateStandStillTimed_OnEnter(pPed);
			FSM_OnUpdate
				return StateStandStillTimed_OnUpdate(pPed);

	FSM_End
}

bool CTaskDoNothing::ProcessMoveSignals()
{
	if( m_bEnableTimeslicing && GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_AllowTaskDoNothingTimeslicing) )
	{
		// check if we need an update next frame
		if( (m_iDurationMs >= 0 && m_Timer.IsOutOfTime()) || (m_iNumFramesToRun > 0 && m_iNumFramesRunSoFar >= m_iNumFramesToRun - 1) )
		{
			CPed* pPed = GetPed();
			if(pPed)
			{
				// force an update next frame
				pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
			}
		}

		switch(GetState())
		{
			case State_StandStillTimed:
				return StandStillTimed_OnProcessMoveSignals();
			default:
				break;
		}
	}
	return false;
}

CTask::FSM_Return CTaskDoNothing::StateInitial_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	return FSM_Continue;
}
CTask::FSM_Return CTaskDoNothing::StateInitial_OnUpdate(CPed * UNUSED_PARAM(pPed))
{
	if(m_iDurationMs < 0 && m_iNumFramesToRun == 0)
	{
		SetState(State_StandStillForever);
	}
	else
	{
		SetState(State_StandStillTimed);
	}

	return FSM_Continue;
}
CTask::FSM_Return CTaskDoNothing::StateStandStillForever_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	return FSM_Continue;
}
CTask::FSM_Return CTaskDoNothing::StateStandStillForever_OnUpdate(CPed * pPed)
{
	DoStandStill(pPed);

	// Quit out if we're the local player running the do nothing as the top most default task with player control,
	// should be running the player on foot task
	if (pPed->IsLocalPlayer() && pPed->GetPlayerInfo() && !pPed->GetPlayerInfo()->AreControlsDisabled() && !GetParent() && pPed->GetPedIntelligence()->GetTaskDefault() == this)
	{
		return FSM_Quit;
	}

	if( m_bEnableTimeslicing && GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_AllowTaskDoNothingTimeslicing) )
	{
		// NOTE: No need for ProcessMoveSignals
		ActivateTimeslicing(pPed);
	}

	return FSM_Continue;
}
CTask::FSM_Return CTaskDoNothing::StateStandStillTimed_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	return FSM_Continue;
}
CTask::FSM_Return CTaskDoNothing::StateStandStillTimed_OnUpdate(CPed * pPed)
{
	DoStandStill(pPed);

	if( !m_bEnableTimeslicing || ! GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_AllowTaskDoNothingTimeslicing) )
	{
		m_iNumFramesRunSoFar++;
	}

	// Run for a specified number of frames
	if(m_iNumFramesToRun != 0)
	{
		if(m_iNumFramesRunSoFar >= m_iNumFramesToRun)
		{
			return FSM_Quit;
		}
	}
	// Or else run for a time interval
	else if(m_Timer.IsOutOfTime())
	{
		return FSM_Quit;
	}

	if( m_bEnableTimeslicing && GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_AllowTaskDoNothingTimeslicing) )
	{
		RequestProcessMoveSignalCalls();
		ActivateTimeslicing(pPed);
	}

	return FSM_Continue;
}

bool CTaskDoNothing::StandStillTimed_OnProcessMoveSignals()
{
	m_iNumFramesRunSoFar++;

	return true;
}

void CTaskDoNothing::DoStandStill(CPed* pPed)
{
	if(m_bMakeMountStandStill && pPed->GetMyMount())
	{
		CPed* pMount = pPed->GetMyMount();
		CPedIntelligence& mountIntel = *pMount->GetPedIntelligence();
		CTask* pMainTask = mountIntel.GetTaskActive();
		if(pMainTask && pMainTask->GetTaskType() != CTaskTypes::TASK_DO_NOTHING)
		{
			// The mount is having some other main task, so it's probably best to
			// not try to force a movement task on it. May have been scripted to do something.
		}
		else
		{
			// Check if the mount already has a TASK_MOVE_STAND_STILL.
			CTask* pMoveTask = mountIntel.GetGeneralMovementTask();
			if(pMoveTask && pMoveTask->GetTaskType() == CTaskTypes::TASK_MOVE_STAND_STILL)
			{
				// Yes, don't have to do anything.
			}
			else
			{
				// Try to give CTaskMoveStandStill to the mount.
				CTaskMoveStandStill* pNewTask = rage_new CTaskMoveStandStill;
				mountIntel.AddTaskMovement(pNewTask);
				pMoveTask = mountIntel.GetGeneralMovementTask();
			}

			// Update CTaskMoveStandStill if we've got it.
			if(pMoveTask && pMoveTask->GetTaskType() == CTaskTypes::TASK_MOVE_STAND_STILL)
			{
				CTaskMoveInterface* pMoveInterface = pMoveTask->GetMoveInterface();
				pMoveInterface->SetCheckedThisFrame(true);
				//pMoveInterface->UpdateProgress(m_pBackupMovementSubtask);
			}
		}
	}

	// Only set the moveblend ratio to still if this ped has no movement task which is moving
	CTaskMove * pTask = pPed->GetPedIntelligence()->GetActiveSimplestMovementTask();
	if(!pTask || !pTask->IsTaskMoving())
	{
		pPed->GetMotionData()->SetDesiredMoveBlendRatio(MOVEBLENDRATIO_STILL);
	}
}

void CTaskDoNothing::ActivateTimeslicing(CPed* pPed)
{
	pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate);
}

CTaskInfo *CTaskDoNothing::CreateQueriableState() const
{
    return rage_new CClonedDoNothingInfo(m_iDurationMs);
}

CTask *CClonedDoNothingInfo::CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity))
{
    return rage_new CTaskDoNothing(GetDuration());
}

//****************************************************************
//	CTaskPause
//****************************************************************

CTaskPause::CTaskPause( const int iPauseTime, const s32 iTimeFlag) 
: m_iPauseTime(iPauseTime)
, m_TimerFlags(iTimeFlag)
{
	SetInternalTaskType(CTaskTypes::TASK_PAUSE);
}

CTaskPause::~CTaskPause()
{

}

CTaskInfo*	CTaskPause::CreateQueriableState() const
{
	return rage_new CClonedPauseInfo((u32)m_iPauseTime, (u32)m_TimerFlags);
}

void CTaskPause::OnCloneTaskNoLongerRunningOnOwner()
{
	// force the timer to quit
	m_iPauseTime = 0;
}

aiTask::FSM_Return CTaskPause::UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent)
{
	return UpdateFSM(iState, iEvent);
}

CTaskFSMClone* CTaskPause::CreateTaskForClonePed(CPed* UNUSED_PARAM(pPed))
{
	CTaskPause *newTask = rage_new CTaskPause(m_iPauseTime, m_TimerFlags);

	return newTask;
}

CTaskFSMClone* CTaskPause::CreateTaskForLocalPed(CPed* pPed)
{
	return CreateTaskForClonePed(pPed);
}

#if !__FINAL
const char * CTaskPause::GetStaticStateName( s32 iState )
{
	Assert(iState>=State_Start&&iState<=State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_GameTimerPause",
		"State_SystemTimerPause",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

CTask::FSM_Return CTaskPause::Start_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	// Entry point for the task.
	taskAssertf(GetFlags().IsFlagSet(TF_GameTimer) != GetFlags().IsFlagSet(TF_SystemTimer), "CTaskPause: Only one flag can be set. Defaulting to game timer");

	if(GetFlags().IsFlagSet(TF_GameTimer))
	{
		SetState(State_GameTimerPause);	
	}
	else
	{
		if(GetFlags().IsFlagSet(TF_SystemTimer))	
		{
			SetState(State_SystemTimerPause);
		}
		else
		{
			SetState(State_GameTimerPause);	//will set the timer to game timer if neither timer flags are set.
		}
	}

	return FSM_Continue;
}
//Game timer 
void CTaskPause::GameTimerPause_OnEnter()
{
	m_GameTimer.Set(fwTimer::GetTimeInMilliseconds(),m_iPauseTime);
}

CTask::FSM_Return CTaskPause::GameTimerPause_OnUpdate()
{
	if(taskVerifyf(m_GameTimer.IsSet(), "CTaskPause::GameTimerPause_OnUpdate: Timer is already set, terminating pause task" ))
	{
		if (m_GameTimer.IsOutOfTime() || m_iPauseTime == 0)
		{
			SetState(State_Finish);
		}
	}
	else
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}

//System timer
void CTaskPause::SystemTimerPause_OnEnter()
{
	m_SystemTimer.Set(fwTimer::GetTimeInMilliseconds(),m_iPauseTime);
}

CTask::FSM_Return CTaskPause::SystemTimerPause_OnUpdate()
{
	if(taskVerifyf(m_SystemTimer.IsSet(), "CTaskPause::SystemTimerPause_OnUpdate: Timer is already set, terminating pause task" ))
	{
		if (m_SystemTimer.IsOutOfTime() || m_iPauseTime == 0)
		{
			SetState(State_Finish);
		}
	}
	else
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}


CTask::FSM_Return CTaskPause::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		// Entrance state for the task.
		FSM_State(State_Start)
		FSM_OnUpdate
		return Start_OnUpdate(pPed);

	//Check the game timer for pausing the task
	FSM_State(State_GameTimerPause)
		FSM_OnEnter
		GameTimerPause_OnEnter();
	FSM_OnUpdate
		GameTimerPause_OnUpdate();

	//Check the system timer for pausing the task
	FSM_State(State_SystemTimerPause)
		FSM_OnEnter
		SystemTimerPause_OnEnter();
	FSM_OnUpdate
		SystemTimerPause_OnUpdate();

	FSM_State(State_Finish)
		FSM_OnUpdate
		return FSM_Quit;
	FSM_End
}

CTaskFSMClone* CClonedPauseInfo::CreateCloneFSMTask()
{
	return rage_new CTaskPause(m_iPauseTime, m_iTimerFlags);
}

//****************************************************************
//	CTaskCrouchToggle
//****************************************************************

CTaskCrouchToggle::CTaskCrouchToggle(int nSwitchOn)
: m_nToggleType(nSwitchOn)
{
	SetInternalTaskType(CTaskTypes::TASK_CROUCH_TOGGLE);
}

CTaskCrouchToggle::~CTaskCrouchToggle()
{
}

#if !__FINAL
const char * CTaskCrouchToggle::GetStaticStateName( s32 iState )
{
	Assert(iState>=State_Start&&iState<=State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Crouch_Off",
		"State_Crouch_On",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL




CTask::FSM_Return CTaskCrouchToggle::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		// Entrance state for the task.
		FSM_State(State_Start)
		FSM_OnUpdate
		return Start_OnUpdate(pPed);

	// In a crouched position.
	FSM_State(State_CrouchOn)
		FSM_OnEnter
		CrouchOn_OnEnter(pPed);
	FSM_OnUpdate
		SetState(State_Finish);
	return FSM_Continue;

	// In a non-crouched position.
	FSM_State(State_CrouchOff)
		FSM_OnEnter
		CrouchOff_OnEnter(pPed);
	FSM_OnUpdate
		SetState(State_Finish);
	return FSM_Continue;

	// Quit the task.
	FSM_State(State_Finish)
		FSM_OnUpdate
		return FSM_Quit;
	FSM_End
}

// Helper functions for FSM update above:

CTask::FSM_Return CTaskCrouchToggle::Start_OnUpdate(CPed* pPed)
{
	// Entry point for the task. Toggle the crouching state by switching to the appropriate state.
	if((m_nToggleType==TOGGLE_CROUCH_OFF || m_nToggleType==TOGGLE_CROUCH_AUTO) && pPed->GetIsCrouching())
	{
		SetState(State_CrouchOff);
	}
	else if((m_nToggleType==TOGGLE_CROUCH_ON || m_nToggleType==TOGGLE_CROUCH_AUTO) && !pPed->GetIsCrouching())
	{
		SetState(State_CrouchOn);
	}
	else
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}

void CTaskCrouchToggle::CrouchOn_OnEnter(CPed* pPed)
{
	// Set the appropriate crouch mode for this state.
	pPed->SetIsCrouching(true);
}

void CTaskCrouchToggle::CrouchOff_OnEnter(CPed* pPed)
{
	// Set the appropriate crouch mode for this state.
	pPed->SetIsCrouching(false);
}


//////////////////////////////
//CTaskComplexControlMovement
//////////////////////////////

const float CTaskComplexControlMovement::ms_fMaxWalkRoundEntityDuration = 8.0f;
bank_bool CTaskComplexControlMovement::ms_bCanClimbLadderAsSubtask = true;
bank_bool CTaskComplexControlMovement::ms_bForceAllowClimbLadderAsSubtask = false;

//-------------------------------------------------------------------------
// Constructor
//-------------------------------------------------------------------------
CTaskComplexControlMovement::CTaskComplexControlMovement( CTask* pMoveTask, CTask* pSubTask, s32 iTerminationType, float fWarpTimer, bool bMoveTaskAlreadyRunning, u32 iFlags )
: m_pBackupMovementSubtask(pMoveTask),
m_pMainSubTask(pSubTask),
m_iTerminationType(iTerminationType),
m_fWarpTimer(fWarpTimer),
m_fTimeBeenWalkingRoundEntity(0.0f),
m_bNewMainTask(false),
m_bNewMoveTask(false),
m_bWaitingForSubtaskToAbort(false),
m_bLeavingVehicle(false),
m_bClimbingLadder(false),
m_bLastClimbingLadderFailed(false),
m_bMovementTaskCompleted(false),
m_bMoveTaskAlreadyRunning(bMoveTaskAlreadyRunning),
m_iFlags(iFlags)
{
	SetInternalTaskType(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT);

	AssertMsg(m_pBackupMovementSubtask, "m_pBackupMovementSubtask is NULL in constructor.");
}

//-------------------------------------------------------------------------
// Destructor, remove all tasks instanced
//-------------------------------------------------------------------------
CTaskComplexControlMovement::~CTaskComplexControlMovement( void )
{
	if( m_pBackupMovementSubtask )
	{
		delete m_pBackupMovementSubtask;
		m_pBackupMovementSubtask = NULL;
	}

	if( m_pMainSubTask )
	{
		delete m_pMainSubTask;
		m_pMainSubTask = NULL;
	}
}

//-------------------------------------------------------------------------
// Returns true if the movement task this task is interested in is up and running
//-------------------------------------------------------------------------
bool	CTaskComplexControlMovement::IsMovementTaskRunning()
{
	const CPed* pMovementTaskPed = GetMovementTaskPed();

	if( (!m_bNewMoveTask) && m_pBackupMovementSubtask.Get() &&
		GetIsFlagSet(aiTaskFlags::HasBegun) && 
		!GetIsFlagSet(aiTaskFlags::IsAborted) && 
		pMovementTaskPed->GetPedIntelligence()->GetGeneralMovementTask() && 
		(pMovementTaskPed->GetPedIntelligence()->GetGeneralMovementTask()->GetTaskType() == m_pBackupMovementSubtask->GetTaskType()) )
	{
		return true;
	}
	return false;
}

//-------------------------------------------------------------------------
// Returns true if the movement task this task is interested in is up and running
//-------------------------------------------------------------------------
CTask*	CTaskComplexControlMovement::GetRunningMovementTask( const CPed* /*pPed*/ )
{
	Assert(m_pBackupMovementSubtask);

	const CPed* pMovementTaskPed = GetMovementTaskPed();

	if( GetIsFlagSet(aiTaskFlags::HasBegun) && 
		!GetIsFlagSet(aiTaskFlags::IsAborted) && 
		m_pBackupMovementSubtask.Get() &&
		pMovementTaskPed->GetPedIntelligence()->GetGeneralMovementTask() && 
		pMovementTaskPed->GetPedIntelligence()->GetGeneralMovementTask()->GetTaskType() == m_pBackupMovementSubtask->GetTaskType() )
	{
		return pMovementTaskPed->GetPedIntelligence()->GetGeneralMovementTask();
	}
	return NULL;
}

//-------------------------------------------------------------------------
// Creates the movement and any subtask
//-------------------------------------------------------------------------
aiTask* CTaskComplexControlMovement::BeginMovementTasks(CPed* pPed)
{
	// Check if we're riding (giving movement tasks to a different ped),
	// and if so, bail out if the mount is running some main task itself.
	if(ShouldYieldToMount(*pPed))
	{
		return NULL;
	}

	const CPed* pMovementTaskPed = GetMovementTaskPed();

	if (m_bMoveTaskAlreadyRunning)
	{
		if (m_pBackupMovementSubtask)
		{
			m_pBackupMovementSubtask = (CTask*)m_pBackupMovementSubtask->Copy();
		}
		m_bMoveTaskAlreadyRunning = false;
	}
	else
	{
		// This is the first update, so begin the movement task
		if( m_pBackupMovementSubtask )
		{
			aiTask* pClone = m_pBackupMovementSubtask->Copy();

			taskAssertf(!pClone || (pPed->GetMyMount() != pMovementTaskPed) || pClone->GetTaskType() != CTaskTypes::TASK_MOVE_PLAYER,
					"CTaskMovePlayer given to a ped's mount through CTaskComplexControlMovement, check the code using this task (%s, state %s).",
					GetParent() ? GetParent()->GetTaskName() : "none", GetParent() ? GetParent()->GetStateName(GetParent()->GetState()) : "?");

			pMovementTaskPed->GetPedIntelligence()->AddTaskMovement( pClone );
			if( !pMovementTaskPed->GetPedIntelligence()->GetGeneralMovementTask() )
			{
				return NULL;
			}
		}
	}


	Assert(pMovementTaskPed->GetPedIntelligence()->GetGeneralMovementTask());
	Assert(pMovementTaskPed->GetPedIntelligence()->GetGeneralMovementTask()->GetTaskType() == m_pBackupMovementSubtask->GetTaskType() );

	// Mark the movement task as still in use this frame
	CTaskMoveInterface* pMoveInterface = pMovementTaskPed->GetPedIntelligence()->GetGeneralMovementTask()->GetMoveInterface();
	pMoveInterface->SetCheckedThisFrame(true);
	pMoveInterface->UpdateProgress(m_pBackupMovementSubtask);

#if !__FINAL
	pMoveInterface->SetOwner(this);
#endif

	if( m_pMainSubTask )
	{
		return m_pMainSubTask->Copy();
	}

	return GenerateDefaultTask(pPed);
}



bool CTaskComplexControlMovement::IsValidForMotionTask( CTaskMotionBase& task) const
{
	bool isValid = !task.IsInWater() || (GetPed() && !GetPed()->GetPedConfigFlag( CPED_CONFIG_FLAG_DrownsInWater ));

	return isValid;
}

bool CTaskComplexControlMovement::HandlesRagdoll(const CPed* pPed) const
{
	//Check if the main sub task can handle the ragdoll.
	const CTask* pMainSubTask = m_pMainSubTask;
	if(pMainSubTask && pMainSubTask->HandlesRagdoll(pPed))
	{
		return true;
	}
	
	//Call the base class version.
	return CTaskComplex::HandlesRagdoll(pPed);
}

//-------------------------------------------------------------------------
// Creates the movement and any subtask
//-------------------------------------------------------------------------
aiTask* CTaskComplexControlMovement::CreateFirstSubTask(CPed* pPed)
{
	// If in a vehicle, first exit before carrying out any movement
	if( pPed->GetMyVehicle() && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) )
	{
		m_bLeavingVehicle = true;
		float fMoveBlendRatio = m_pBackupMovementSubtask->GetMoveInterface()->GetMoveBlendRatio();
 		const bool bCloseDoor = CPedMotionData::GetIsStill(fMoveBlendRatio) || CPedMotionData::GetIsWalking(fMoveBlendRatio);
		VehicleEnterExitFlags vehicleFlags;
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::WarpIfDoorBlocked );
		if (!bCloseDoor)
		{
			vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontCloseDoor);
		}
 		return rage_new CTaskLeaveAnyCar(0, vehicleFlags);
	}
	return BeginMovementTasks(pPed);
}

//-------------------------------------------------------------------------
// Creates the movement subtask
//-------------------------------------------------------------------------
aiTask* CTaskComplexControlMovement::CreateNextSubTask(CPed* pPed)
{
	// If the ped has just used a ladder, maybe notify the movement task
	if( m_bClimbingLadder )
	{
		m_bClimbingLadder = false;

		// If we had a sub-task before climbing the ladder, try to use it
		if(m_pMainSubTask)
		{
			m_bNewMainTask = true;
		}
	}

	// If the ped has just left a vehicle, begin the movement task from scratch
	if( m_bLeavingVehicle )
	{
		m_bLeavingVehicle = false;
		if (pPed->GetIsInVehicle())
		{
			taskWarningf("Ped failed to exit car in CTaskComplexControlMovement!");
			return NULL;
		}
		else
		{
			return BeginMovementTasks(pPed);
		}
	}

	const CPed* pMovementTaskPed = GetMovementTaskPed();

	// If the task should terminate when the main task finishes
	// OR if either task finishes,
	// OR if both tasks have terminated and the movement task has already terminated
	// abort the movement task and return NULL to terminate this task
	if( m_bMovementTaskCompleted || m_iTerminationType == TerminateOnSubtask || m_iTerminationType == TerminateOnEither ||
		( !m_pBackupMovementSubtask && TerminateOnBoth ) || m_bWaitingForSubtaskToAbort )
	{
		// 		Assert( m_pMainSubTask );
		// 		Assert( m_pMainSubTask->GetTaskType() == GetSubTask()->GetTaskType() );
		// If the task is yet to create the movement task, nothing to do to abort the task
		if( !GetIsFlagSet(aiTaskFlags::IsAborted) && pMovementTaskPed->GetPedIntelligence()->GetGeneralMovementTask() )
		{
			// In certain situations terminating the movement task will cause glitches by introducing a single frame
			// of the default TaskSimpleMoveDoNothing.  Avoid this by having a special-case for these tasks.
			const bool bTerminate = (GetParent()==NULL) || (GetParent()->GetTaskType()!=CTaskTypes::TASK_COMPLEX_USE_MOBILE_PHONE_AND_MOVEMENT);

			if(!m_bNewMoveTask)
			{
				// Abort and terminate the movement task
				Assert(pMovementTaskPed->GetPedIntelligence()->GetGeneralMovementTask());
				Assertf(pMovementTaskPed->GetPedIntelligence()->GetGeneralMovementTask()->GetTaskType() == m_pBackupMovementSubtask->GetTaskType(),
					"GetGeneralMovementTask()->GetTaskType() : %i, m_pBackupMovementSubtask->GetTaskType() : %i", pMovementTaskPed->GetPedIntelligence()->GetGeneralMovementTask()->GetTaskType(), m_pBackupMovementSubtask->GetTaskType()
					);
			}

			if(bTerminate)
			{
				pMovementTaskPed->GetPedIntelligence()->GetGeneralMovementTask()->MakeAbortable( CTask::ABORT_PRIORITY_IMMEDIATE, NULL );
				pMovementTaskPed->GetPedIntelligence()->GetGeneralMovementTask()->m_Flags.SetFlag(aiTaskFlags::HasFinished);
			}
			else
			{
				pMovementTaskPed->GetPedIntelligence()->GetGeneralMovementTask()->GetMoveInterface()->SetCheckedThisFrame(true);
			}
		}
		return NULL;
	}
	else
	{
		if( m_pMainSubTask && m_pMainSubTask->GetTaskType() == GetSubTask()->GetTaskType() )
		{
			delete m_pMainSubTask;
			m_pMainSubTask = NULL;
		}
		return GenerateDefaultTask(pPed);
	}
}


//-------------------------------------------------------------------------
// Creates the movement subtask
//-------------------------------------------------------------------------
aiTask* CTaskComplexControlMovement::ControlSubTask(CPed* pPed)
{
	AssertMsg( m_pBackupMovementSubtask.Get(), "m_pBackupMovementSubtask is NULL in ControlSubTask" );

	if( !GetSubTask() )
	{
		pPed->SetPedResetFlag( CPED_RESET_FLAG_GestureAnimsAllowed, true );
	}

	const CPed* pMovementTaskPed = GetMovementTaskPed();

	// If currently leaving a vehicle before beginning the movement task, wait for that task to end
	if( m_bLeavingVehicle )
	{
		return GetSubTask();
	}
	else if( pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle() )
	{
		if( GetSubTask()->MakeAbortable( CTask::ABORT_PRIORITY_URGENT, NULL ) ||
			GetSubTask()->MakeAbortable( CTask::ABORT_PRIORITY_IMMEDIATE, NULL ) )
		{
			m_bLeavingVehicle = true;
			float fMoveBlendRatio = m_pBackupMovementSubtask->GetMoveInterface()->GetMoveBlendRatio();
 			const bool bCloseDoor = CPedMotionData::GetIsStill(fMoveBlendRatio) || CPedMotionData::GetIsWalking(fMoveBlendRatio);
			VehicleEnterExitFlags vehicleFlags;
			vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::WarpIfDoorBlocked );
			if (!bCloseDoor)
			{
				vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontCloseDoor);
			}
	 		return rage_new CTaskLeaveAnyCar(0, vehicleFlags);
		}
	}
	else if( pPed->PopTypeIsRandom() && !pPed->IsLawEnforcementPed() && (!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_WillRemainOnBoatAfterMissionEnds )) &&
		pPed->GetGroundPhysical() && pPed->GetGroundPhysical()->GetIsTypeVehicle() &&
		((CVehicle*)pPed->GetGroundPhysical())->GetVehicleType()==VEHICLE_TYPE_BOAT)
	{
		CEventMustLeaveBoat eventLeaveBoat((CBoat*)pPed->GetGroundPhysical());
		pPed->GetPedIntelligence()->AddEvent(eventLeaveBoat);
	}

	// If waiting for the subtask to abort
	if( m_bWaitingForSubtaskToAbort )
	{
		if( GetSubTask()->MakeAbortable( CTask::ABORT_PRIORITY_URGENT, NULL ) )
		{
			return NULL;
		}
		else
		{
			return GetSubTask();
		}
	}

	// If a warp timer is set, warp after time
	if( m_fWarpTimer > 0.0f )
	{
		m_fWarpTimer -= GetTimeStep();
		if( m_fWarpTimer <= 0.0f )
		{
			if( GetSubTask()->MakeAbortable( CTask::ABORT_PRIORITY_URGENT, NULL) )
			{
				// TODO: if the task is running on a horse rider, this should probably operate on the horse.
				Assert(m_pBackupMovementSubtask->GetMoveInterface() && !CPedMotionData::GetIsStill(m_pBackupMovementSubtask->GetMoveInterface()->GetMoveBlendRatio()));
				Vector3 vWarpTarget = m_pBackupMovementSubtask->GetMoveInterface()->GetTarget();
				float fHeading = pPed->GetTransform().GetHeading();
				pPed->Teleport(vWarpTarget, fHeading, true);
				return NULL;
			}
			m_fWarpTimer = 0.001f;
		}
	}

	CTask * pActiveMoveTask = pMovementTaskPed->GetPedIntelligence()->GetMovementResponseTask();
	if(pActiveMoveTask && pActiveMoveTask->GetTaskType()==CTaskTypes::TASK_WALK_ROUND_ENTITY)
	{
		m_fTimeBeenWalkingRoundEntity += GetTimeStep();
		if(m_fTimeBeenWalkingRoundEntity >= ms_fMaxWalkRoundEntityDuration)
		{
			if(pActiveMoveTask->MakeAbortable( ABORT_PRIORITY_URGENT, NULL))
			{
				((CTaskWalkRoundEntity*)pActiveMoveTask)->SetQuitNextFrame();
			}
		}
	}
	else
	{
		m_fTimeBeenWalkingRoundEntity = 0.0f;
	}

	// If the task was previously aborted restart the movement task
	// PH
	if( !IsMovementTaskRunning() && !m_bMovementTaskCompleted )
	{
		pMovementTaskPed->GetPedIntelligence()->AddTaskMovement( m_pBackupMovementSubtask->Copy() );
		m_bNewMoveTask = false;
	}

	// Update the movement task if its not already completed
	if( !m_bMovementTaskCompleted )
	{
		// Finish immediately if the movement task is no longer valid
		if( !pMovementTaskPed->GetPedIntelligence()->GetGeneralMovementTask() ||
			pMovementTaskPed->GetPedIntelligence()->GetGeneralMovementTask()->GetTaskType() != m_pBackupMovementSubtask->GetTaskType() )
		{
			if( GetSubTask()->MakeAbortable( CTask::ABORT_PRIORITY_URGENT, NULL ) )
			{
				return NULL;
			}
		}

		Assert(pMovementTaskPed->GetPedIntelligence()->GetGeneralMovementTask());
		Assert(pMovementTaskPed->GetPedIntelligence()->GetGeneralMovementTask()->GetTaskType() == m_pBackupMovementSubtask->GetTaskType() );

		// Mark the movement task as still in use this frame
		CTaskMoveInterface* pMoveInterface = pMovementTaskPed->GetPedIntelligence()->GetGeneralMovementTask()->GetMoveInterface();
		pMoveInterface->SetCheckedThisFrame(true);
		pMoveInterface->UpdateProgress(m_pBackupMovementSubtask);
#if !__FINAL
		pMoveInterface->SetOwner(this);
#endif

		// If the task has finished, check the termination conditions
		if( pMovementTaskPed->GetPedIntelligence()->GetGeneralMovementTask()->GetIsFlagSet(aiTaskFlags::HasFinished) )
		{
			// If the task should terminate when the main task finishes
			// OR if either task finishes,
			// OR if both tasks have terminated and the movement task has already terminated
			// abort the movement task and return NULL to terminate this task
			if( m_iTerminationType == TerminateOnMovementTask || m_iTerminationType == TerminateOnEither ||
				( !m_pMainSubTask && m_iTerminationType == TerminateOnBoth ) )
			{
				if( GetSubTask()->MakeAbortable( CTask::ABORT_PRIORITY_URGENT, NULL ) )
				{
					return NULL;
				}
				m_bWaitingForSubtaskToAbort = true;
			}
			m_bMovementTaskCompleted = true;
		}
	}

	// Climbing ladder
	if( m_bClimbingLadder )
	{
		// If we're running the correct task, just return this until it calls CreateNextSubtask()
		if( GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_USE_LADDER_ON_ROUTE )
		{
			return GetSubTask();
		}
		// Otherwise try to create the correct subtask & return it here
		else
		{
			if( !GetSubTask() || GetSubTask()->MakeAbortable( CTask::ABORT_PRIORITY_URGENT, NULL ) ||
				GetSubTask()->MakeAbortable( CTask::ABORT_PRIORITY_IMMEDIATE, NULL ) )
			{
				CTaskUseLadderOnRoute * pUseLadderTask = rage_new CTaskUseLadderOnRoute(m_fLadderMBR, m_vLadderTarget, m_fLadderHeading, NULL);
				return pUseLadderTask;
			}
		}
	}

	// If a new main task has been set, change to that task
	if( m_bNewMainTask )
	{
		if( GetSubTask()->MakeAbortable( CTask::ABORT_PRIORITY_URGENT, NULL ) )
		{
			m_bNewMainTask = false;

			// The if check here was added to support SetNewMainSubtask(NULL), which seems like it
			// should be valid to do.
			if(m_pMainSubTask)
			{
				return m_pMainSubTask->Copy();
			}
			else
			{
				// If we were to return NULL here, the whole task would end (due to how
				// CTaskComplex works), which is not what we want. Instead, we generate
				// a CTaskDoNothing, which is what BeginMovementTasks() would have done if
				// the subtask had been NULL to begin with.
				return GenerateDefaultTask(pPed);
			}
		}
	}

	return GetSubTask();
}

//-------------------------------------------------------------------------
// Replace the current movement task
//-------------------------------------------------------------------------
void CTaskComplexControlMovement::SetNewMoveTask( CTask* pSubTask )
{
	if( m_pBackupMovementSubtask )
	{
		delete m_pBackupMovementSubtask;
		m_pBackupMovementSubtask = NULL;
	}
	m_pBackupMovementSubtask = pSubTask;
	m_bNewMoveTask = true;
	m_bMovementTaskCompleted = false;
}


//-------------------------------------------------------------------------
// Replace the current subtask
//-------------------------------------------------------------------------
void CTaskComplexControlMovement::SetNewMainSubtask( CTask* pSubTask )
{
	Assertf( m_iTerminationType == TerminateOnMovementTask, "Can only change main subtask if the movement task is in control of termination!" );

	// Can't change task while still waiting for the current task to complete
	if( m_bWaitingForSubtaskToAbort )
	{
		return;
	}

	if( m_pMainSubTask )
	{
		delete m_pMainSubTask;
		m_pMainSubTask = NULL;
	}
	m_pMainSubTask = pSubTask;
	m_bNewMainTask = true;
}

//-------------------------------------------------------------------------
// Sets the target of the current movement task
//-------------------------------------------------------------------------
void CTaskComplexControlMovement::SetTarget( const CPed* pPed, const Vector3& vTarget, const bool bForce )
{
	Assert(CTask::IsTaskPtr(this));

	if( IsMovementTaskRunning() )
	{
		const CPed* pMovementTaskPed = GetMovementTaskPed();
		CTask* pGeneralMovementTask = pMovementTaskPed->GetPedIntelligence()->GetGeneralMovementTask();

		Assert(pGeneralMovementTask && pGeneralMovementTask->GetMoveInterface());
		// Set the target for both the active task and the stored version
		pGeneralMovementTask->GetMoveInterface()->SetTarget(pPed, VECTOR3_TO_VEC3V(vTarget), bForce);
	}
	Assert(m_pBackupMovementSubtask && m_pBackupMovementSubtask->GetMoveInterface());
	m_pBackupMovementSubtask->GetMoveInterface()->SetTarget(pPed, VECTOR3_TO_VEC3V(vTarget), bForce);
}

//-------------------------------------------------------------------------
// Sets the target of the current movement task
//-------------------------------------------------------------------------
void CTaskComplexControlMovement::SetTargetRadius( const CPed* UNUSED_PARAM(pPed), const float fTargetRadius )
{
	if( IsMovementTaskRunning() )
	{
		const CPed* pMovementTaskPed = GetMovementTaskPed();
		CTask* pGeneralMovementTask = pMovementTaskPed->GetPedIntelligence()->GetGeneralMovementTask();

		// Set the target for both the active task and the stored version
		pGeneralMovementTask->GetMoveInterface()->SetTargetRadius(fTargetRadius);
	}
	m_pBackupMovementSubtask->GetMoveInterface()->SetTargetRadius(fTargetRadius);
}

//-------------------------------------------------------------------------
// Sets the target of the current movement task
//-------------------------------------------------------------------------
void CTaskComplexControlMovement::SetSlowDownDistance( const CPed* UNUSED_PARAM(pPed), const float fSlowDownDistance )
{
	if( IsMovementTaskRunning() )
	{
		const CPed* pMovementTaskPed = GetMovementTaskPed();
		CTask* pGeneralMovementTask = pMovementTaskPed->GetPedIntelligence()->GetGeneralMovementTask();

		// Set the target for both the active task and the stored version
		pGeneralMovementTask->GetMoveInterface()->SetSlowDownDistance(fSlowDownDistance);
	}
	m_pBackupMovementSubtask->GetMoveInterface()->SetSlowDownDistance(fSlowDownDistance);
}


//-------------------------------------------------------------------------
// Gets the target of the current movement task
//-------------------------------------------------------------------------
Vector3 CTaskComplexControlMovement::GetTarget( void ) const
{
	// Mark the movement task as still in use this frame
	return m_pBackupMovementSubtask->GetMoveInterface()->GetTarget();
}

//-------------------------------------------------------------------------
// Sets the target of the current movement task
//-------------------------------------------------------------------------
void CTaskComplexControlMovement::SetMoveBlendRatio( const CPed* UNUSED_PARAM(pPed), const float fMoveBlend )
{
	if( IsMovementTaskRunning() )
	{
		const CPed* pMovementTaskPed = GetMovementTaskPed();
		CTask* pGeneralMovementTask = pMovementTaskPed->GetPedIntelligence()->GetGeneralMovementTask();
		// Set the target for both the active task and the stored version
		pGeneralMovementTask->GetMoveInterface()->SetMoveBlendRatio(fMoveBlend);
		CTask* pSubTask = pGeneralMovementTask->GetSubTask();
		while(pSubTask)
		{
			if( pSubTask->GetMoveInterface() )
			{
				pSubTask->GetMoveInterface()->SetMoveBlendRatio(fMoveBlend);
			}
			pSubTask = pSubTask->GetSubTask();
		}
	}

	m_pBackupMovementSubtask->GetMoveInterface()->SetMoveBlendRatio(fMoveBlend);
}

//-------------------------------------------------------------------------
// Gets the target of the current movement task
//-------------------------------------------------------------------------
float CTaskComplexControlMovement::GetMoveBlendRatio() const
{
	// Mark the movement task as still in use this frame
	return m_pBackupMovementSubtask->GetMoveInterface()->GetMoveBlendRatio();
}

//-------------------------------------------------------------------------
// Function to create cloned task information
//-------------------------------------------------------------------------
CTaskInfo *CTaskComplexControlMovement::CreateQueriableState() const
{
    CTaskInfo *taskInfo = 0;

    if(m_pBackupMovementSubtask)
    {
        taskInfo = m_pBackupMovementSubtask->CreateQueriableState();
		taskInfo->SetType(m_pBackupMovementSubtask->GetTaskType());
    }

    if(!taskInfo)
    {
        taskInfo = rage_new CTaskInfo();
    }

    return taskInfo;
}

//-------------------------------------------------------------------------
// Checks if we are riding on a mount that has its own task
//-------------------------------------------------------------------------
bool CTaskComplexControlMovement::ShouldYieldToMount(const CPed& ped)
{
	const CPed* pMount = ped.GetMyMount();
	if(pMount)
	{
		CTask* pMainTask = pMount->GetPedIntelligence()->GetTaskActive();
		if(pMainTask && pMainTask->GetTaskType() != CTaskTypes::TASK_DO_NOTHING && pMainTask->GetTaskType() != CTaskTypes::TASK_GETTING_MOUNTED)
		{
			return true;
		}
	}
	return false;
}

aiTask* CTaskComplexControlMovement::GenerateDefaultTask( CPed* UNUSED_PARAM(pPed) )
{
	// Note: we pass in false here to the makeMountStandStill parameter, because in this case,
	// we're already handing the horse a movement task, so we don't want the CTaskDoNothing
	// running on the rider telling it to stand still!
	const int iRunTimeMS = -1; // indefinite
	const int iNumFramesToRun = 0; // indefinite
	const bool bMakeMountStandStill = false;
	const bool bEnableTimeslicing = false;
	return rage_new CTaskDoNothing(iRunTimeMS, iNumFramesToRun, bMakeMountStandStill, bEnableTimeslicing);
}

bool CTaskComplexControlMovement::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	// If the task is yet to create the movement task, nothing to do to abort the task
	if( !IsMovementTaskRunning() )
	{
		return true;
	}

	const CPed* pMovementTaskPed = GetMovementTaskPed();

	// Abort and terminate the movement task
	Assert(pMovementTaskPed->GetPedIntelligence()->GetGeneralMovementTask());
	Assert(pMovementTaskPed->GetPedIntelligence()->GetGeneralMovementTask()->GetTaskType() == m_pBackupMovementSubtask->GetTaskType() );

	// Only return true if task was abortable
	if(pMovementTaskPed->GetPedIntelligence()->GetGeneralMovementTask()->MakeAbortable( iPriority, pEvent ))
	{
		// Don't finish the movement task directly, it will be automatically removed if not checked by the main task
		//pPed->GetPedIntelligence()->GetGeneralMovementTask()->SetHasFinished(true);
		pMovementTaskPed->GetPedIntelligence()->GetGeneralMovementTask()->GetMoveInterface()->SetCheckedThisFrame(false);
		return true;
	}

	return false;
}


CPed* CTaskComplexControlMovement::GetMovementTaskPed()
{
	CPed* pPed = GetPed();
	CPed* pMount = pPed->GetMyMount();
	return pMount ? pMount : pPed;
}

//-------------------------------------------------------------------------
// Helper class used to store and query a list of tasks
//-------------------------------------------------------------------------
CTaskList::CTaskList()
{
	int i;
	for(i=0;i<MAX_LIST_SIZE;i++)
	{
		m_tasks[i] = NULL;
	}
}

//-------------------------------------------------------------------------
// Destructor
//-------------------------------------------------------------------------
CTaskList::~CTaskList()
{
	Flush();
}

//-------------------------------------------------------------------------
// Adds a task into the first free slot
//-------------------------------------------------------------------------
bool CTaskList::AddTask(aiTask* pTask)
{
	int i;
	for(i=0;i<MAX_LIST_SIZE;i++)
	{
		if(0==m_tasks[i])
		{
			m_tasks[i]=pTask;
			return true;
		}
	}
	Assertf(false, "Too many Tasks in this TaskList");
	delete pTask;
	return false;
}


//-------------------------------------------------------------------------
// Returns the most recently added task
//-------------------------------------------------------------------------
aiTask* CTaskList::GetLastAddedTask() const
{
	for(int i=0;i<MAX_LIST_SIZE;i++)
	{
		if(0==m_tasks[i])
		{
			if( (i - 1) >= 0 )
			{
				return m_tasks[i-1];
			}
			else
			{
				return NULL;
			}
		}
	}
	return NULL;
}


//-------------------------------------------------------------------------
// returns true if the list contains the task
//-------------------------------------------------------------------------
bool CTaskList::Contains(aiTask* p)
{
	int i;
	for(i=0;i<MAX_LIST_SIZE;i++)
	{
		if(p && m_tasks[i] && p->GetTaskType()==m_tasks[i]->GetTaskType()) 
		{
			return true;
		}
	}
	return false;
}


//-------------------------------------------------------------------------
// Returns true if the list contains a task of the type
//-------------------------------------------------------------------------
aiTask* CTaskList::Contains(const int iTaskType)
{
	int i;
	for(i=0;i<MAX_LIST_SIZE;i++)
	{
		if(m_tasks[i] && (iTaskType==m_tasks[i]->GetTaskType())) 
		{
			return m_tasks[i];
		}
	}
	return NULL;
}


//-------------------------------------------------------------------------
// Fills the task list from the one passed
//-------------------------------------------------------------------------
void CTaskList::From( const CTaskList& taskList )
{
	int i;
	for(i=0;i<MAX_LIST_SIZE;i++)
	{
		if(taskList.m_tasks[i])
		{
			m_tasks[i]=taskList.m_tasks[i]->Copy();
		}
		else
		{
			m_tasks[i]=NULL;
		}
	}
}


//-------------------------------------------------------------------------
// Adds a task into a specific slot
//-------------------------------------------------------------------------
void CTaskList::AddTask(const int index, aiTask* pTask)
{
	if(index<MAX_LIST_SIZE)
	{
		if(m_tasks[index])
		{
			delete m_tasks[index];
		}
		m_tasks[index]=pTask;
	}
	else
	{
		Assert(false);
		delete pTask;
	}
}

//-------------------------------------------------------------------------
// Flush all the tasks
//-------------------------------------------------------------------------
void CTaskList::Flush()
{
	int i;
	for(i=0;i<MAX_LIST_SIZE;i++)
	{
		if(m_tasks[i])
		{
			delete m_tasks[i];
		}
		m_tasks[i]=NULL;
	}
}

////////////////////////////////////////
// Structure to store task sequence data
////////////////////////////////////////

FW_INSTANTIATE_CLASS_POOL_SPILLOVER(CTaskSequenceList, CTaskSequenceList::MAX_STORAGE, 0.73f, atHashString("CTaskSequenceList",0x40eac531));

CTaskSequenceList::CTaskSequenceList()
: m_iRepeatMode(REPEAT_NOT)
, m_iRefCount(0)
, m_bPreventMigration(false)
{
}

CTaskSequenceList::CTaskSequenceList(const CTaskSequenceList& other)
: m_iRepeatMode(other.m_iRepeatMode)
, m_iRefCount(other.m_iRefCount)
, m_bPreventMigration(false)
{
	m_taskList.From(other.m_taskList);
}

CTaskSequenceList::~CTaskSequenceList()
{
	// Delete all the tasks in the list
	Flush();
}

void CTaskSequenceList::Flush()
{
	m_taskList.Flush();
	m_iRepeatMode = REPEAT_NOT;
}

CTaskSequenceList* CTaskSequenceList::Copy() const 
{
	CTaskSequenceList* pTaskSequenceList = rage_new CTaskSequenceList();
	aiAssert(pTaskSequenceList);
	pTaskSequenceList->m_taskList.From(m_taskList);
	pTaskSequenceList->m_iRepeatMode = m_iRepeatMode;
	return pTaskSequenceList;
}

bool CTaskSequenceList::AddTask(aiTask* pTask)
{
	if (m_taskList.AddTask(pTask))
	{
		return true;
	}
	aiAssertf(false, "Too many Tasks in this Sequence");
	return false;
}

//////////////////////////////////////
//PERFORM SEQUENCE OF MOVEMENT TASKS
//////////////////////////////////////

CTaskMoveSequence::CTaskMoveSequence()
: CTaskMove(MOVEBLENDRATIO_STILL),
m_iProgress(0),
m_iRepeatMode(REPEAT_NOT),
m_iRepeatProgress(0),
m_bCanBeEmptied(false),
m_iRefCount(0)
{
	SetInternalTaskType(CTaskTypes::TASK_COMPLEX_MOVE_SEQUENCE);
}

CTaskMoveSequence::~CTaskMoveSequence()
{
	Flush();
}

void CTaskMoveSequence::Flush()
{
	m_taskList.Flush();
	m_iProgress=0;
	m_iRepeatMode=REPEAT_NOT;
	m_iRepeatProgress=0;
}

int CTaskMoveSequence::GetTaskTypeInternal() const
{
	return CTaskTypes::TASK_COMPLEX_MOVE_SEQUENCE;
}

#if !__FINAL
void CTaskMoveSequence::Debug() const
{
	GetSubTask()->Debug();
}
#endif

#if !__FINAL
atString CTaskMoveSequence::GetName() const
{
	if(0==m_taskList.GetTask(m_iProgress))
	{
		return atString("Sequence");
	}
	else
	{
		return m_taskList.GetTask(m_iProgress)->GetName();
	}
}
#endif

CTask::FSM_Return CTaskMoveSequence::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Initial)
		FSM_OnEnter
		return Initial_OnEnter();
	FSM_OnUpdate
		return Initial_OnUpdate();

	FSM_State(State_PlayingSequence)
		FSM_OnEnter
		return PlayingSequence_OnEnter();
	FSM_OnUpdate
		return PlayingSequence_OnUpdate();
	FSM_End
}

CTask::FSM_Return CTaskMoveSequence::Initial_OnEnter()
{
	m_iProgress = 0;
	m_iRepeatProgress = 0;
	return FSM_Continue;
}
CTask::FSM_Return CTaskMoveSequence::Initial_OnUpdate()
{
	SetState(State_PlayingSequence);
	return FSM_Continue;
}

CTask::FSM_Return CTaskMoveSequence::PlayingSequence_OnEnter()
{
	aiTask * pNewTask = CreateFirstSubTaskInSequence(m_iProgress);
	SetNewTask(pNewTask);
	return pNewTask ? FSM_Continue : FSM_Quit;
}
CTask::FSM_Return CTaskMoveSequence::PlayingSequence_OnUpdate()
{
	if(GetSubTask())
	{
		if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
		{
			aiTask * pNewTask = CreateNextSubTaskInSequence(m_iProgress, m_iRepeatProgress);
			SetNewTask(pNewTask);
			return pNewTask ? FSM_Continue : FSM_Quit;
		}
		else
		{
			return FSM_Continue;
		}
	}
	else
	{
		return FSM_Quit;
	}
}

aiTask* CTaskMoveSequence::CreateNextSubTaskInSequence(int & iProgress, int & iRepeatProgress)
{
	aiTask* pNextSubTask=0;

	iProgress++;
	if(REPEAT_NOT==m_iRepeatMode)
	{
		if((CTaskList::MAX_LIST_SIZE==iProgress) || (0==m_taskList.GetTask(iProgress)))
		{
			//Reached the end of the sequence so finish.
			pNextSubTask=0;
		}
		else
		{
			//Return next task in sequence.
			pNextSubTask=m_taskList.GetTask(iProgress)->Copy();
			Assert( pNextSubTask->GetTaskType() == m_taskList.GetTask(iProgress)->GetTaskType() );
		}
	}
	else
	{
		if((CTaskList::MAX_LIST_SIZE==iProgress) || (0==m_taskList.GetTask(iProgress)))
		{
			//Reached the end of the sequence.
			//Go back to the beginning.
			//Increment number of completed sequence loops.
			iProgress=0;
			iRepeatProgress++;
		}		
		Assert(m_taskList.GetTask(iProgress));

		if(REPEAT_FOREVER==m_iRepeatMode)
		{
			pNextSubTask=m_taskList.GetTask(iProgress)->Copy();
			Assert( pNextSubTask->GetTaskType() == m_taskList.GetTask(iProgress)->GetTaskType() );
		}
		else
		{
			if(iRepeatProgress==m_iRepeatMode)
			{
				pNextSubTask=0;
			}
			else
			{
				pNextSubTask=m_taskList.GetTask(iProgress)->Copy();
				Assert( pNextSubTask->GetTaskType() == m_taskList.GetTask(iProgress)->GetTaskType() );
			}
		}
	}

	return pNextSubTask;
}

aiTask* CTaskMoveSequence::CreateFirstSubTaskInSequence(int & iProgress)
{
	if(0==m_taskList.GetTask(iProgress))
	{
		return 0;
	}
	else
	{
		aiTask* pTaskClone = m_taskList.GetTask(iProgress)->Copy();
		Assert( pTaskClone->GetTaskType() == m_taskList.GetTask(iProgress)->GetTaskType() );
		return pTaskClone;
	}
}

bool CTaskMoveSequence::AddTask(aiTask* pTask)
{
	if( m_taskList.AddTask(pTask) )
	{
		return true;
	}
	Assertf(false, "Too many Tasks in this Sequence");
	delete pTask;
	return false;
}

void CTaskMoveSequence::AddTask(const int index, aiTask* pTask)
{
	m_taskList.AddTask(index, pTask);
}

int CTaskSequences::ms_iActiveSequence=-1;
bool CTaskSequences::ms_bIsOpened[CTaskSequences::MAX_NUM_SEQUENCE_TASKS];
CTaskSequenceList CTaskSequences::ms_TaskSequenceLists[CTaskSequences::MAX_NUM_SEQUENCE_TASKS];

void CTaskSequences::Init()
{
	ms_iActiveSequence=-1;
	int i;
	for(i=0;i<MAX_NUM_SEQUENCE_TASKS;i++)
	{
		ms_bIsOpened[i]=false;
		ms_TaskSequenceLists[i].Flush();
		Assert(ms_TaskSequenceLists[i].IsEmpty());
	}
}

void CTaskSequences::Shutdown(void)
{
	int i;
	for(i=0;i<MAX_NUM_SEQUENCE_TASKS;i++)
	{
		ms_bIsOpened[i]=false;
		ms_TaskSequenceLists[i].Flush();
		Assert(ms_TaskSequenceLists[i].IsEmpty());
	}
}

s32 CTaskSequences::GetAvailableSlot(bool bSequenceForMission)
{
	s32 iSequenceID=-1;
	s32 i;
	s32 StartIndex, EndIndex;

	CompileTimeAssert(MAX_NUM_SEQUENCE_TASKS == 128);	//	If MAX_NUM_SEQUENCE_TASKS ever changes then DividingIndex will probably need to be adjusted.
	const s32 DividingIndex = 32;						//	I'm not even sure if this function is ever called with bSequenceForMission=false any more.

	if (bSequenceForMission)
	{	//	Half the sequences can only be used by mission scripts
		StartIndex = DividingIndex;
		EndIndex = MAX_NUM_SEQUENCE_TASKS;
	}
	else
	{	//	The other sequences are for brains and ambient scripts
		StartIndex = 0;
		EndIndex = DividingIndex;
	}
	for(i=StartIndex;i<EndIndex;i++)
	{
		if(!ms_bIsOpened[i])
		{
			const CTaskSequenceList& sequence=ms_TaskSequenceLists[i];
			if(sequence.IsEmpty())
			{
				iSequenceID=i;
				break;
			}
		}
	}
	return iSequenceID;
}

/////////////////////
// TRIGGER IK LOOK //
/////////////////////

CTaskTriggerLookAt::CTaskTriggerLookAt(const CEntity* pEntity, s32 time, eAnimBoneTag offsetBoneTag, const Vector3& offsetPos, s32 flags, float speed, s32 blendTime, CIkManager::eLookAtPriority priority)
{
	m_pEntity = pEntity;
	m_time = time;
	m_offsetBoneTag = offsetBoneTag;
	m_offsetPos = offsetPos;
	m_flags = flags;
	m_speed = speed;
	m_blendTime = blendTime;

	m_priority = priority;

	if (m_pEntity)
	{
		m_nonNullEntity = true;
	}
	else
	{
		m_nonNullEntity = false;
	}

	if (m_offsetBoneTag!=-1)
	{
		taskAssert(m_pEntity);
		taskAssert(m_pEntity->GetIsTypePed());
	}

	SetInternalTaskType(CTaskTypes::TASK_TRIGGER_LOOK_AT);
}  


CTaskTriggerLookAt::~CTaskTriggerLookAt()
{
} 

aiTask* CTaskTriggerLookAt::Copy() const
{
	eAnimBoneTag offsetBoneTag = m_offsetBoneTag;
	s32 time = m_time;

	if (m_pEntity.Get() == NULL)
	{
		if (m_offsetBoneTag > BONETAG_INVALID)
		{
			offsetBoneTag = BONETAG_INVALID;
			time = 100;
		}
	}

	return rage_new CTaskTriggerLookAt(m_pEntity, time, offsetBoneTag, m_offsetPos, m_flags, m_speed, m_blendTime, m_priority);
}

//update the local state
CTask::FSM_Return CTaskTriggerLookAt::UpdateFSM( const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		//Task start
		FSM_State(State_start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_triggerLook)
			FSM_OnUpdate
				return TriggerLook_OnUpdate(pPed);

		FSM_State(State_finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

CTaskInfo* CTaskTriggerLookAt::CreateQueriableState() const
{
	if (m_nonNullEntity)
	{
		const CEntity* pEntity = m_pEntity.Get();

		if (AssertVerify(pEntity))
		{
			netObject *pNetObj = NetworkUtils::GetNetworkObjectFromEntity(pEntity);

			if (pNetObj)
			{
				return rage_new CClonedLookAtInfo(pNetObj->GetObjectID(), m_time, m_flags);
			}
			else
			{
				return rage_new CClonedLookAtInfo(VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()), m_time, m_flags);
			}
		}
	}
	else
	{
		return rage_new CClonedLookAtInfo(m_offsetPos, m_time, m_flags);
	}

	return NULL;
}

//check if we're in a vehicle, and ditch if necessary
CTask::FSM_Return CTaskTriggerLookAt::Start_OnUpdate()
{
	//check if the target entity has been deleted
	if(m_nonNullEntity && m_pEntity.Get()==NULL)
	{	
		SetState(State_finish); //Quit immediately if the entity no longer exists
	}
	else
	{
		SetState(State_triggerLook); // start the IK task
	}

	return FSM_Continue;
}

//trigger the IK task and move to the exit state
CTask::FSM_Return CTaskTriggerLookAt::TriggerLook_OnUpdate(CPed *pPed)
{

	taskAssertf(!(m_nonNullEntity && m_pEntity.Get()==NULL),"Cannot look at target entity. Entity has been deleted.");

	// trigger the ik look
	pPed->GetIkManager().LookAt(0, m_pEntity, m_time, m_offsetBoneTag, &m_offsetPos, m_flags, m_blendTime, m_blendTime, m_priority);
	SetState(State_finish);

	return FSM_Continue;
}

#if !__FINAL
// return the name of the given task as a string
const char * CTaskTriggerLookAt::GetStaticStateName( s32 iState )
{
	// Make sure FSM state is valid
	taskAssert(iState <= State_finish);
	static const char* aStateNames[] = 
	{
		"State_start",
		"State_triggerIk",
		"State_finish"
	};

	return aStateNames[iState];
}
#endif //!__FINAL


CTask  *CClonedLookAtInfo::CreateLocalTask(fwEntity *UNUSED_PARAM(pEntity))
{
	s32 time = (m_Time == 0) ? -1 : (s32)m_Time;

	if (m_LookAtEntity)
	{
		netObject* pNetObj = NetworkInterface::GetNetworkObject(m_EntityID);

		CEntity* pEntity = pNetObj ? pNetObj->GetEntity() : NULL;

		if (AssertVerify(pNetObj) && AssertVerify(pEntity))
		{
			eAnimBoneTag boneTag = pEntity->GetIsTypePed() ? BONETAG_HEAD : BONETAG_INVALID;
			return rage_new CTaskTriggerLookAt(pEntity, time, boneTag, Vector3(VEC3_ZERO), m_Flags);
		}
	}
	else
	{
		return rage_new CTaskTriggerLookAt(NULL, time, BONETAG_INVALID, m_Position, m_Flags);
	}

	return NULL;
}

///////////////////
// CLEAR IK LOOK //
///////////////////

CTaskClearLookAt::CTaskClearLookAt()
{
	SetInternalTaskType(CTaskTypes::TASK_CLEAR_LOOK_AT);
}  


CTaskClearLookAt::~CTaskClearLookAt()
{

}

//Update the local state machine
CTask::FSM_Return CTaskClearLookAt::UpdateFSM( const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_abortLook)
			FSM_OnUpdate
				AbortLook_OnUpdate(pPed);

		FSM_State(State_finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

//Abort the currently running look at task
CTask::FSM_Return CTaskClearLookAt::AbortLook_OnUpdate(CPed* pPed)
{
	if (pPed->GetIkManager().IsLooking())
	{
		pPed->GetIkManager().AbortLookAt(500);
	}
	SetState(State_finish);

	return FSM_Continue;
}

#if !__FINAL
// return the name of the given task as a string
const char * CTaskClearLookAt::GetStaticStateName( s32 iState )
{
	// Make sure FSM state is valid
	taskAssert(iState <= State_finish);
	static const char* aStateNames[] = 
	{
		"State_abortLook",
		"State_finish"
	};

	return aStateNames[iState];
}
#endif //!__FINAL


//////////////////////
//USE MOBILE PHONE
//////////////////////

static float PHONE_APPEAR_PHASE				= 0.1f;
static float PHONE_APPEAR_PHASE_FROM_TEXT	= 0.0f;
static float PHONE_DISAPPEAR_PHASE			= 0.9f;

CTaskComplexUseMobilePhone::CTaskComplexUseMobilePhone(const int iDuration)
: m_iDuration(iDuration),
m_iModelIndex(fwModelId::MI_INVALID),
m_bIsAborting(false),
m_bQuit(false),
m_bModelReferenced(false),
m_bFromTexting(false),
m_bIsQuitting(false),
m_iPreviousSelectedWeapon(0),
m_bGivenPhone(false)
{
	SetInternalTaskType(CTaskTypes::TASK_COMPLEX_USE_MOBILE_PHONE);
}

CTaskComplexUseMobilePhone::~CTaskComplexUseMobilePhone()
{
	if( m_bModelReferenced )
	{
		CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(m_iModelIndex)))->RemoveRef();
		m_bModelReferenced = false;
	}
}

void CTaskComplexUseMobilePhone::CleanUp()
{
	RemovePhoneModel(GetPed());
}

//
// name:		RemovePhoneModel
// description:	Remove phone model from ped and replace with weapon
void CTaskComplexUseMobilePhone::RemovePhoneModel(CPed* pPed)
{
	// Prevent ped from switching weapons when removing the weapon so we don't switch without 'swapping' the weapon
	pPed->SetPedResetFlag( CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching, true );

	weaponAssert(pPed->GetInventory() && pPed->GetWeaponManager());
	pPed->GetInventory()->RemoveWeapon(OBJECTTYPE_OBJECT);

	// Reselect our previous weapon (it will be drawn when selecting the weapon in task player)
	if (m_iPreviousSelectedWeapon != 0)
	{
		pPed->GetWeaponManager()->EquipWeapon(m_iPreviousSelectedWeapon);
	}
	m_bGivenPhone = false;
}

void CTaskComplexUseMobilePhone::Stop(CPed* pPed)
{
	((void)pPed);

	if(!m_bQuit)
	{
		m_bQuit=true;
	}
}

CTaskComplexUseMobilePhone::FSM_Return CTaskComplexUseMobilePhone::ProcessPreFSM()
{
	GetPed()->SetPedResetFlag( CPED_RESET_FLAG_TemporarilyBlockWeaponEquipping, true );
	return FSM_Continue;
}

void CTaskComplexUseMobilePhone::DoAbort(const AbortPriority UNUSED_PARAM(iPriority), const aiEvent* pEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.
	if( pPed->IsLocalPlayer() )
	{
		bool bDontTellScript = false;
		if( pEvent && ((CEvent*)pEvent)->GetEventType() == EVENT_SCRIPT_COMMAND )
		{
			const CEventScriptCommand* pScriptEvent = static_cast<const CEventScriptCommand*>(pEvent);
			if( pScriptEvent->GetTask() && 
				( pScriptEvent->GetTask()->GetTaskType() == CTaskTypes::TASK_COMPLEX_USE_MOBILE_PHONE || 
				pScriptEvent->GetTask()->GetTaskType() == CTaskTypes::TASK_COMPLEX_USE_MOBILE_PHONE_AND_MOVEMENT ) )
			{
				bDontTellScript = true;
			}
		}
		if( !bDontTellScript )
		{
			CPhoneMgr::SetRemoveFlags(CPhoneMgr::WANTS_PHONE_REMOVED_PHONE_TASK_ABORTED_S1);
		}

//		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_PlayerPressedTriangleWhileHangingUpPhone, false );
	}

	// Remove this model unless this is the phone script getting hte player to get out of the car
	if((!CPhoneMgr::IsDisplayed() || CPhoneMgr::GetGoingOffScreen()) || 
		(pEvent == NULL) || (((CEvent*)pEvent)->GetEventType() != EVENT_SCRIPT_COMMAND ))
	{
		RemovePhoneModel(pPed);
	}
}

aiTask* CTaskComplexUseMobilePhone::CreateNextSubTask(CPed* pPed)
{
	aiTask* pNextSubTask=0;
	switch(GetSubTask()->GetTaskType())
	{
	case CTaskTypes::TASK_SIMPLE_PHONE_IN:
		if(m_iDuration>=0)
		{
			m_timer.Set(fwTimer::GetTimeInMilliseconds(),m_iDuration);
		}
		pNextSubTask=rage_new CTaskRunClip(GetPhoneClipGroup(pPed), CLIP_STD_PHONE_TALK, INSTANT_BLEND_IN_DELTA, NORMAL_BLEND_OUT_DELTA, -1, 0, CTaskTypes::TASK_SIMPLE_PHONE_TALK); //,"PhoneChat");
		break;
	case CTaskTypes::TASK_SIMPLE_PHONE_TALK:
		pNextSubTask=rage_new CTaskRunClip(GetPhoneClipGroup(pPed), CLIP_STD_PHONE_OUT, INSTANT_BLEND_IN_DELTA, NORMAL_BLEND_OUT_DELTA, -1, 0, CTaskTypes::TASK_SIMPLE_PHONE_OUT); //,"PhoneOut");
		break;
	case CTaskTypes::TASK_SIMPLE_PHONE_OUT:
		if(m_bQuit)
		{
			RemovePhoneModel(pPed);
			pNextSubTask=0;
		}
		else if(m_bIsAborting)
		{
			RemovePhoneModel(pPed);
			return rage_new CTaskPause(1000);
		}
		else
		{
			RemovePhoneModel(pPed);
			pNextSubTask=0;			
		}
		break;
	case CTaskTypes::TASK_PAUSE:
		m_bIsAborting=false;
		m_timer.Unset();
		if(m_bQuit)
		{
			pNextSubTask=0;
		}
		else
		{
			pNextSubTask=CreateFirstSubTask(pPed);
		}
		break;
	default:
		Assert(false);
		break;
	}

	return pNextSubTask;
}

aiTask* CTaskComplexUseMobilePhone::CreateFirstSubTask(CPed* pPed)
{
	CClipHelper* pClipPlayer = pPed->GetMovePed().GetBlendHelper().FindClipByClipId(CLIP_STD_PHONE_TEXT);
	float fStartPhase = 0.0f;
	if( pClipPlayer )
	{
		m_bFromTexting = true;
		fStartPhase = pClipPlayer->GetPhase();
	}

	const bool bUsingMobilePhone = CTaskPlayerOnFoot::CheckForUseMobilePhone(*pPed);
	if (bUsingMobilePhone || pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_MOBILE_PHONE))
	{
		m_bFromTexting = true;
	}

//#if 0 // CS
	fwMvClipId clipId = ( m_bFromTexting/* || pPed->HasPhoneEquipped() */) ? CLIP_STD_PHONE_IN_FROM_TEXT : CLIP_STD_PHONE_IN;
//#else
//	fwMvClipId clipId = CLIP_STD_PHONE_IN;
//#endif // 0
	if(pPed->IsPlayer())
	{
		pPed->GetPlayerInfo()->GetTargeting().ClearLockOnTarget();
	}

	// Remove weapon object if it is not the phone
#if 0 // CS
	if(!pPed->HasPhoneEquipped())
	{
		pPed->GetInventory().RemoveWeaponObject(pPed);
	}
#endif // 0

	m_iModelIndex = CPedPhoneComponent::GetPhoneModelIndexSafe(pPed->GetPhoneComponent());
	fwModelId modelId((strLocalIndex(m_iModelIndex)));

	if(!modelId.IsValid())
	{
		Assertf(false, "Mobile phone doesn't exist");
		return NULL;
	}

	// If this is the player in a vehicle, then make sure this vehicle is started.
	if(pPed->IsPlayer() && pPed->GetMyVehicle() && pPed->GetMyVehicle()->GetDriver() == pPed)
	{
		pPed->GetMyVehicle()->SwitchEngineOn(true);
	}

	// Make sure the cellphone model has loaded
	if (!CModelInfo::HaveAssetsLoaded(modelId))
	{
		CModelInfo::RequestAssets(modelId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
		// The pause task is running at the start to give the task the time to load the mobile phone
		if(m_bFromTexting)
		{
			CTaskRunClip* pTask = rage_new CTaskRunClip(GetPhoneClipGroup(pPed), CLIP_STD_PHONE_TEXT, SLOW_BLEND_IN_DELTA, SLOW_BLEND_OUT_DELTA, -1, 0);
			if (pTask)
			{
				pTask->SetStartPhase(fStartPhase);
			}
			return pTask;
		}
		else
		{
			return rage_new CTaskPause(300);
		}
	}
	CModelInfo::GetBaseModelInfo(modelId)->AddRef();
	m_bModelReferenced = true;

	CTaskRunClip* pTask = rage_new CTaskRunClip(GetPhoneClipGroup(pPed), clipId, FAST_BLEND_IN_DELTA, INSTANT_BLEND_OUT_DELTA, -1, 0, CTaskTypes::TASK_SIMPLE_PHONE_IN); //,"PhoneIn");
	return pTask;
}

aiTask* CTaskComplexUseMobilePhone::ControlSubTask(CPed* pPed)
{
	if( GetIsFlagSet(aiTaskFlags::IsAborted) )
	{
		if( GetSubTask()->MakeAbortable( CTask::ABORT_PRIORITY_URGENT, NULL ) )
		{
			RemovePhoneModel(pPed);
			return NULL;
		}
	}
	aiTask* pReturnTask=GetSubTask();

	// Add a player movement task in the movement slot if this is a player and one is not already present
	// To allow the player to move whilst using the phone
	if( pPed->IsPlayer() )
	{
		if( GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT )
		{
			if( ((CTaskComplexControlMovement*)GetParent())->GetBackupCopyOfMovementSubtask()->GetTaskType() != CTaskTypes::TASK_MOVE_PLAYER &&
				((CTaskComplexControlMovement*)GetParent())->GetBackupCopyOfMovementSubtask()->GetTaskType() != CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH )
			{
				((CTaskComplexControlMovement*)GetParent())->SetNewMoveTask(rage_new CTaskMovePlayer());
			}
		}
	}

	pPed->SetPedResetFlag( CPED_RESET_FLAG_UsingMobilePhone, true );
	//pPed->m_PedResetFlags.SetHeadIKBlocked(2);

	fwMvClipId clipId = m_bFromTexting ? CLIP_STD_PHONE_IN_FROM_TEXT : CLIP_STD_PHONE_IN;
	if(m_bModelReferenced)
	{
		const CClipHelper* pRunningClip = NULL;
		if( GetSubTask() && (GetSubTask()->GetTaskType() == CTaskTypes::TASK_SIMPLE_PHONE_IN ||
			GetSubTask()->GetTaskType() == CTaskTypes::TASK_SIMPLE_PHONE_OUT ) )
		{
			CTaskRunClip* pRunClipTask = static_cast<CTaskRunClip*>(GetSubTask());
			pRunningClip = pRunClipTask->GetClipIfPlaying();
		}

		const CClipHelper* pStartClip = (pRunningClip && pRunningClip->GetClipId() == clipId) ? pRunningClip : NULL;
		const CClipHelper* pEndClip = (pRunningClip && pRunningClip->GetClipId() == CLIP_STD_PHONE_OUT) ? pRunningClip : NULL;
		
		if(pStartClip)
		{
			const float fAppearPhase = m_bFromTexting ? PHONE_APPEAR_PHASE_FROM_TEXT : PHONE_APPEAR_PHASE;
			if(pStartClip->GetTime() >= fAppearPhase || m_bFromTexting )
			{
				fwModelId modelId((strLocalIndex(m_iModelIndex)));
				Assert( CModelInfo::HaveAssetsLoaded(modelId));

				const CBaseModelInfo* modelInfo = CModelInfo::GetBaseModelInfo(modelId);


				// Need to do this to equip the phone
				pPed->SetPedResetFlag( CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching, false );

				if( !m_bGivenPhone )
				{
					m_bGivenPhone= true;
					
					m_iPreviousSelectedWeapon = pPed->GetWeaponManager()->GetSelectedWeaponHash();

					CWeaponItem* pItem = pPed->GetInventory() ? pPed->GetInventory()->AddWeapon(OBJECTTYPE_OBJECT) : NULL;

					if (pItem)
					{
						// Set the correct model
						pItem->SetModelHash(modelInfo->GetModelNameHash());

						if (pPed->GetWeaponManager() && pPed->GetWeaponManager()->EquipWeapon(OBJECTTYPE_OBJECT))
						{
							pPed->GetWeaponManager()->CreateEquippedWeaponObject();

							if (pPed->GetWeaponManager()->GetEquippedWeaponObject())
							{
								pPed->GetWeaponManager()->GetEquippedWeaponObject()->m_nObjectFlags.bAmbientProp = true;
							}
						}
						else
						{
							pPed->GetInventory()->RemoveWeapon(OBJECTTYPE_OBJECT);
						}
					}

					// Reset the flag to true to prevent switching weapons
					pPed->SetPedResetFlag( CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching, true );
				}
			}
		}	
		else if(pEndClip)
		{
			if(pEndClip->GetPhase() >= PHONE_DISAPPEAR_PHASE && 
				(pEndClip->GetPhase() - pEndClip->GetPhaseUpdateAmount()) < PHONE_DISAPPEAR_PHASE)
			{
				RemovePhoneModel(pPed);
			}
		}
		else if(!m_bIsQuitting && (m_timer.IsOutOfTime() || m_bQuit))
			//if(m_timer.IsOutOfTime())
		{
			if( GetSubTask()->MakeAbortable( CTask::ABORT_PRIORITY_IMMEDIATE, NULL ) )
			{
				pReturnTask=rage_new CTaskRunClip(GetPhoneClipGroup(pPed), CLIP_STD_PHONE_OUT, INSTANT_BLEND_IN_DELTA, INSTANT_BLEND_OUT_DELTA, -1, 0, CTaskTypes::TASK_SIMPLE_PHONE_OUT); //,"PhoneOut");
				m_bIsQuitting = true;
			}
			else
			{
				Assert(0);
			}
		}
	}
	// The pause task is running at the start to give the task the time to load the mobile phone
	else if(!m_bIsAborting)
	{
		fwModelId modelId((strLocalIndex(m_iModelIndex)));
		// If the mobile phone has finished loading, we're ready to begin using it so begin the phone in clip
		if (CModelInfo::HaveAssetsLoaded(modelId))
		{
			CModelInfo::GetBaseModelInfo(modelId)->AddRef();
			m_bModelReferenced = true;
			if( GetSubTask()->MakeAbortable( CTask::ABORT_PRIORITY_URGENT, NULL ) )
			{
				pReturnTask=rage_new CTaskRunClip(GetPhoneClipGroup(pPed), clipId, SLOW_BLEND_IN_DELTA, SLOW_BLEND_OUT_DELTA, -1, 0, CTaskTypes::TASK_SIMPLE_PHONE_IN); //,"PhoneIn");
			}
		}
		else
		{
			CModelInfo::RequestAssets(modelId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
		}
	}

	// Keep track if the player presses triangle during putting the phone away, so the main player
	// task can take note of it when it resumes
	/*
	if( pPed->IsLocalPlayer() && GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_SIMPLE_PHONE_OUT )
	{
	CControl *pControl = pPed->GetControlFromPlayer();
	if( pControl && pControl->GetPedEnter().IsPressed() )
	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_PlayerPressedTriangleWhileHangingUpPhone, true );
	}
	*/
	return pReturnTask;
}

fwMvClipSetId CTaskComplexUseMobilePhone::GetPhoneClipGroup( CPed* pPed )
{
	if( pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) || pPed->GetIsCrouching() )
	{
		return CLIP_SET_CELLPHONE_1HANDED;
	}
	else if(pPed->GetMotionData()->GetUsingStealth())
	{
		return CLIP_SET_CELLPHONE_STEALTH;
	}
	else
	{
		return CLIP_SET_CELLPHONE;
	}
}


/////////////////////
//SET DECISIONMAKER
/////////////////////

//local state machine
CTask::FSM_Return CTaskSetCharDecisionMaker::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_start)
			FSM_OnUpdate
				return Start_OnUpdate(pPed);
		FSM_State(State_finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

CTask::FSM_Return CTaskSetCharDecisionMaker::Start_OnUpdate(CPed* pPed)
{
	taskAssertf(pPed->IsPlayer() == false, "CTaskSimpleSetCharDecisionMaker::ProcessPed - Can't change a player's decision maker");

	pPed->GetPedIntelligence()->SetDecisionMakerId(m_uDM);

	SetState(State_finish);

	return FSM_Continue;
}

#if !__FINAL
// return the name of the given task as a string
const char * CTaskSetCharDecisionMaker::GetStaticStateName( s32 iState )
{
	// Make sure FSM state is valid
	taskAssert(iState <= State_finish);
	static const char* aStateNames[] = 
	{
		"State_start", 
		"State_finish"
	};

	return aStateNames[iState];
}
#endif //!__FINAL


////////////////////////////////
//SET PED SPHERE DEFENSIVE AREA
////////////////////////////////

CClonedSetPedDefensiveAreaInfo::CClonedSetPedDefensiveAreaInfo(bool clearDefensiveArea, const Vector3 &centrePoint, const float radius) :
m_ClearDefensiveArea(clearDefensiveArea)
, m_Centre(centrePoint)
, m_Radius(radius)
{
}

CTask *CClonedSetPedDefensiveAreaInfo::CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity))
{
    if(m_ClearDefensiveArea)
    {
        return rage_new CTaskSetPedDefensiveArea();
    }
    else
    {
        return rage_new CTaskSetPedDefensiveArea(m_Centre, m_Radius);
    }
}

//local state machine
CTask::FSM_Return CTaskSetPedDefensiveArea::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTaskInfo *CTaskSetPedDefensiveArea::CreateQueriableState() const
{
    return rage_new CClonedSetPedDefensiveAreaInfo(m_bClearDefensiveArea, m_vCenter, m_fRadius);
}

void CTaskSetPedDefensiveArea::ReadQueriableState(CClonedFSMTaskInfo *pTaskInfo)
{
    Assert( pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_SET_DEFENSIVE_AREA);
    CClonedSetPedDefensiveAreaInfo *pSetPedDefensiveAreaInfo = static_cast<CClonedSetPedDefensiveAreaInfo*>(pTaskInfo);

    m_bClearDefensiveArea = pSetPedDefensiveAreaInfo->IsClearingDefensiveArea();
    m_vCenter             = pSetPedDefensiveAreaInfo->GetCentrePoint();
    m_fRadius             = pSetPedDefensiveAreaInfo->GetRadius();

	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

void SetPedDefensiveIfNeeded( CPed* pPed )
{
	if(pPed)
	{
		// If our ped is set as will advance movement then change it to defensive
		CCombatBehaviour& pedCombatBehaviour = pPed->GetPedIntelligence()->GetCombatBehaviour();
		if(pedCombatBehaviour.GetCombatMovement() == CCombatData::CM_WillAdvance)
		{
			pedCombatBehaviour.SetCombatMovement(CCombatData::CM_Defensive);
		}
	}
}

CTask::FSM_Return CTaskSetPedDefensiveArea::Start_OnUpdate()
{
	CPed* pPed = GetPed();

	if(m_bClearDefensiveArea)
	{
		pPed->GetPedIntelligence()->GetDefensiveAreaManager()->GetPrimaryDefensiveArea()->Reset();
	}
	else
	{
		pPed->GetPedIntelligence()->GetDefensiveAreaManager()->GetPrimaryDefensiveArea()->SetAsSphere( m_vCenter, m_fRadius, NULL );
		SetPedDefensiveIfNeeded(pPed);
	}

	SetState(State_finish);

	return FSM_Continue;
}

#if !__FINAL
// return the name of the given task as a string
const char * CTaskSetPedDefensiveArea::GetStaticStateName( s32 iState )
{
	// Make sure FSM state is valid
	taskAssert(iState <= State_finish);
	static const char* aStateNames[] = 
	{
		"State_start", 
		"State_finish"
	};

	return aStateNames[iState];
}
#endif //!__FINAL

//////////////////////////
//USE A SEQUENCE OF TASKS
//////////////////////////


//-------------------------------------------------------------------------
// Cloned Task info for task sequences
//-------------------------------------------------------------------------
CClonedTaskSequenceInfo::CClonedTaskSequenceInfo()
: m_ResourceId(0)
, m_Progress1(0)
, m_Progress2(0)
, m_ExitAfterInterrupted(false)
{
}

CClonedTaskSequenceInfo::~CClonedTaskSequenceInfo() 
{
}

CClonedTaskSequenceInfo::CClonedTaskSequenceInfo(u32 /*sequenceId*/
												 ,u32 sequenceResourceId
												 ,u32 progress1
												 ,s32 progress2
												 ,bool exitAfterInterrupted)
: m_ResourceId(0)
, m_Progress1((u8)progress1)
, m_Progress2((s8)progress2)
, m_ExitAfterInterrupted(exitAfterInterrupted)
{
	if (sequenceResourceId != 0)
	{
		m_ResourceId = CScriptResource_SequenceTask::GetSequenceIdFromResourceId(sequenceResourceId);
	}

#if __ASSERT
	if (NetworkInterface::IsGameInProgress())
	{
		taskAssertf(m_ResourceId < (1<<CScriptResource_SequenceTask::SIZEOF_SEQUENCE_ID), 
					"Resource id=%u bigger than %u", m_ResourceId, (1<<CScriptResource_SequenceTask::SIZEOF_SEQUENCE_ID));

		taskAssert(progress1 < (1 << (CClonedTaskSequenceInfo::SIZEOF_PROGRESS1)));
		taskAssert(progress2 < 0 || progress2 < (1 << (CClonedTaskSequenceInfo::SIZEOF_PROGRESS2)));
	}
#endif
}

u32 CClonedTaskSequenceInfo::GetSequenceResourceId() const	
{ 
	return CScriptResource_SequenceTask::GetResourceIdFromSequenceId(m_ResourceId); 
}

CTask*
CClonedTaskSequenceInfo::CreateLocalTask(fwEntity *pEntity)
{
	s32 sequenceId = -1;

	bool bScriptSequenceExists = false;

	CPed* pPed = (CPed*)pEntity;
	CNetObjPed* pPedObj = pPed ? SafeCast(CNetObjPed, pPed->GetNetworkObject()) : NULL;

	if (pPedObj)
	{
		if (m_ResourceId != -1)
		{
			// we may be running the script that created this sequence, use it if so 
			CScriptEntityExtension* extension = pPed->GetExtension<CScriptEntityExtension>();

			if (extension)
			{
				scriptHandler* handler = CTheScripts::GetScriptHandlerMgr().GetScriptHandler(extension->GetScriptInfo()->GetScriptId());

				if (handler)
				{
					scriptResource* pScriptResource = handler->GetScriptResource(GetSequenceResourceId());

					if (pScriptResource && Verifyf(pScriptResource->GetType() == CGameScriptResource::SCRIPT_RESOURCE_SEQUENCE_TASK, "Script sequence resource not found for sequence resource id %d (%s)", GetSequenceResourceId(), pPedObj->GetLogName()))
					{
						sequenceId = pScriptResource->GetReference();

						if (!CTaskSequences::ms_TaskSequenceLists[sequenceId].IsEmpty())
						{
							bScriptSequenceExists = true;
						}
					}
				}
			}
		}

		if (!bScriptSequenceExists)
		{
			// if the ped is not running the script that created this sequence, a temporary one has been created for it which is only to be used locally. Use this instead if it exists.
			sequenceId = pPedObj->GetTemporaryTaskSequenceId();

			if (sequenceId == -1)
			{
				// only script peds send their sequence task data, we can dump the sequence for ambient peds
				Assertf(!pPedObj->IsScriptObject() || pPedObj->IsLocalFlagSet(netObject::LOCALFLAG_BEINGREASSIGNED), "%s has migrated running a sequence task but no temporary sequence exists for the task!", pPedObj->GetLogName());
				return NULL;
			}
		}
	}

	CTaskUseSequence* pSequenceTask = rage_new CTaskUseSequence(sequenceId, GetSequenceResourceId());

	pSequenceTask->SetProgress((u32)m_Progress1, (s32)m_Progress2);
	pSequenceTask->SetExitAfterInterrupted(m_ExitAfterInterrupted);

	if (pPedObj)
	{
		pPedObj->RemoveTemporaryTaskSequence();
	}

	return pSequenceTask;
}

// Note: Each constructor should call register on the static sequence list
CTaskUseSequence::CStaticSequenceHelper::CStaticSequenceHelper(const s32 id)
: m_id(id)
{
	if (m_id != -1)
	{
		taskAssert((m_id>=0) && (m_id<CTaskSequences::MAX_NUM_SEQUENCE_TASKS));
		CTaskSequences::ms_TaskSequenceLists[m_id].Register();
	}
}

CTaskUseSequence::CStaticSequenceHelper::CStaticSequenceHelper(const CStaticSequenceHelper& other)
: m_id(other.m_id)
{
	if (m_id != -1)
	{
		taskAssert((m_id>=0) && (m_id<CTaskSequences::MAX_NUM_SEQUENCE_TASKS));
		CTaskSequences::ms_TaskSequenceLists[m_id].Register();
	}
}

// Calls deregister on it's static tasks sequence list
CTaskUseSequence::CStaticSequenceHelper::~CStaticSequenceHelper()
{
	if (m_id != -1)
	{
		taskAssert((m_id>=0) && (m_id<CTaskSequences::MAX_NUM_SEQUENCE_TASKS));
		CTaskSequences::ms_TaskSequenceLists[m_id].DeRegister();
	}
}

// Static task sequence constructor
CTaskUseSequence::CTaskUseSequence(const s32 sequenceId, const u32 resourceId)
: m_iPrimarySequenceProgress(0)
, m_iSubtaskSequenceProgress(-1)		// May not exist
, m_iRepeatProgress(0)
, m_bRecreateOverNetwork(true)
, m_pTaskSequenceList(NULL)
, m_bInitialiseSubtaskSequenceProgress(true)
, m_bExitAfterInterrupted(false)
, m_StaticSequenceHelper(sequenceId)
, m_sequenceResourceId(resourceId)
, m_bPedCanMigrate(false)
, m_bEarlyOutFromStopExactly(false)
{
	SetInternalTaskType(CTaskTypes::TASK_USE_SEQUENCE);
}

// Dynamically allocated task sequence constructor
CTaskUseSequence::CTaskUseSequence(CTaskSequenceList& TaskSequenceList)
: m_iPrimarySequenceProgress(0)
, m_iSubtaskSequenceProgress(-1)
, m_iRepeatProgress(0)
, m_bRecreateOverNetwork(false)
, m_pTaskSequenceList(&TaskSequenceList)
, m_bInitialiseSubtaskSequenceProgress(true)
, m_bExitAfterInterrupted(false)
, m_StaticSequenceHelper(-1)
, m_sequenceResourceId(0)
, m_bPedCanMigrate(false)
, m_bEarlyOutFromStopExactly(false)
{
	taskAssert(m_pTaskSequenceList);

	SetInternalTaskType(CTaskTypes::TASK_USE_SEQUENCE);
}

// Copy constructor
CTaskUseSequence::CTaskUseSequence(const CTaskUseSequence& other)
: m_iPrimarySequenceProgress(other.m_iPrimarySequenceProgress)
, m_iSubtaskSequenceProgress(other.m_iSubtaskSequenceProgress)
, m_iRepeatProgress(other.m_iRepeatProgress)
, m_bRecreateOverNetwork(other.m_bRecreateOverNetwork)
, m_pTaskSequenceList(NULL)
, m_bInitialiseSubtaskSequenceProgress(true)
, m_bExitAfterInterrupted(false)
, m_StaticSequenceHelper(other.m_StaticSequenceHelper)
, m_sequenceResourceId(other.m_sequenceResourceId)
, m_bPedCanMigrate(false)
, m_bEarlyOutFromStopExactly(other.m_bEarlyOutFromStopExactly)
{
	const CTaskSequenceList* pOtherSeqList = other.m_pTaskSequenceList;
	if (pOtherSeqList)
	{
		taskAssertf(m_StaticSequenceHelper.GetID() == -1, "Task Use Sequence has a dynamically allocated sequence list and a valid static id");
		m_pTaskSequenceList = pOtherSeqList->Copy();
	}
	SetInternalTaskType(CTaskTypes::TASK_USE_SEQUENCE);
}

CTaskUseSequence::~CTaskUseSequence()
{
	// Delete the dynamically allocated task sequence list and all of the tasks within it if it exists
	if (m_pTaskSequenceList)
	{
		delete m_pTaskSequenceList;
		m_pTaskSequenceList = NULL;
	}
}

s32 CTaskUseSequence::GetDefaultStateAfterAbort() const
{
	CTaskScriptedAnimation *pTaskScriptedAnimation = static_cast< CTaskScriptedAnimation * >(FindSubTaskOfType(CTaskTypes::TASK_SCRIPTED_ANIMATION));
	if(pTaskScriptedAnimation)
	{
		m_bExitAfterInterrupted = pTaskScriptedAnimation->IsFlagSet(AF_EXIT_AFTER_INTERRUPTED);
	}

	CTaskRunNamedClip *pTaskRunNamedClip = static_cast< CTaskRunNamedClip * >(FindSubTaskOfType(CTaskTypes::TASK_RUN_NAMED_CLIP));
	if(pTaskRunNamedClip)
	{
		m_bExitAfterInterrupted = pTaskRunNamedClip->IsTaskFlagSet(CTaskClip::ATF_EXIT_AFTER_INTERRUPTED);
	}

	if(FindSubTaskOfType(CTaskTypes::TASK_PARACHUTE))
	{
		m_bExitAfterInterrupted = true;
	}

	return GetState();
}

CTask::FSM_Return CTaskUseSequence::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_RunningTask)
			FSM_OnEnter
				RunningTask_OnEnter();
			FSM_OnUpdate
				return RunningTask_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnEnter
				Finish_OnEnter();
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskUseSequence::Start_OnUpdate()
{
	const bool bIsInjured = GetPed()->IsInjured();
	if (bIsInjured)
	{
		SetState(State_Finish);
		return FSM_Continue;
	}
	else
	{
		SetState(State_RunningTask);
		return FSM_Continue;
	}
}

void CTaskUseSequence::RunningTask_OnEnter()
{
	if(m_bExitAfterInterrupted)
	{
		m_iPrimarySequenceProgress ++;

		m_bExitAfterInterrupted = false;
	}

	s32 iSubTaskType = GetSubTask() ? GetSubTask()->GetTaskType() : CTaskTypes::TASK_INVALID_ID;
	bool bIsSubTaskResumable = (iSubTaskType == CTaskTypes::TASK_ENTER_VEHICLE ||
								iSubTaskType == CTaskTypes::TASK_COMBAT_CLOSEST_TARGET_IN_AREA ||
								iSubTaskType == CTaskTypes::TASK_THREAT_RESPONSE);

	// Do not recreate the subtask if its an enter vehicle task and we had aborted the parent sequence task
	if (!bIsSubTaskResumable || !GetIsFlagSet(aiTaskFlags::IsAborted))
	{
		aiTask* pCloneTask = CreateTask();

		if (pCloneTask)
		{
			// Try to initialise the sequence subtask if there is one (only do this the first time)
			if (m_bInitialiseSubtaskSequenceProgress)
			{
				InitialiseSequenceSubtask(pCloneTask);
				m_bInitialiseSubtaskSequenceProgress = false;
			}

			SetNewTask(pCloneTask);
		}
	}

	// prevent the ped migrating to non-participants of the script that created it if the sequence cannot migrate
	if (GetPed()->GetNetworkObject() && GetTaskSequenceList()->IsMigrationPrevented())
	{
		m_bPedCanMigrate = !GetPed()->GetNetworkObject()->IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_SCRIPT_MIGRATION);
		GetPed()->GetNetworkObject()->SetGlobalFlag(CNetObjGame::GLOBALFLAG_SCRIPT_MIGRATION, true);
	}

	m_bEarlyOutFromStopExactly = false;
	if (m_iPrimarySequenceProgress < CTaskList::MAX_LIST_SIZE - 1)
	{
		// It would be a good idea to consider go to navmesh task here also

		const CTaskSequenceList* pTaskList = GetTaskSequenceList();
		const aiTask* pTask = pTaskList->GetTask(m_iPrimarySequenceProgress);
		if (pTaskList->GetTask(m_iPrimarySequenceProgress + 1) &&
			pTask && pTask->GetTaskType()==CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
		{
			int iMoveTaskType = ((CTaskComplexControlMovement*)pTask)->GetMoveTaskType();
			if (iMoveTaskType == CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL)
			{
				CTaskMoveGoToPointAndStandStill* pBackUpMoveTask = (CTaskMoveGoToPointAndStandStill*) ((CTaskComplexControlMovement*)pTask)->GetBackupCopyOfMovementSubtask();
				if (pBackUpMoveTask && pBackUpMoveTask->GetStopExactly())
				{
					// Ok so... Allow early break out!
					m_bEarlyOutFromStopExactly = true;
				}			
			}
		}
	}
}

CTask::FSM_Return CTaskUseSequence::RunningTask_OnUpdate()
{
	const bool bIsInjured = GetPed()->IsInjured();
	const bool bCanEarlyStartNextSeq = GetPed()->GetPedResetFlag(CPED_RESET_FLAG_IsExactStopSettling) && m_bEarlyOutFromStopExactly;

	if (!bIsInjured && (bCanEarlyStartNextSeq || GetIsFlagSet(aiTaskFlags::SubTaskFinished)))
	{
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_IsExactStopping, false); 
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_IsExactStopSettling, false);
		SetFlag(aiTaskFlags::RestartCurrentState);
		++m_iPrimarySequenceProgress;
	}
	else if (!GetSubTask())	
	{
		// Quit if no subtask was started, this means there were no tasks in the task list, 
		// the task id or task list pointer was invalid or we finished processing all tasks from the list
		// Or if we are injured, don't run any more tasks e.g if we were given a task which doesn't abort for damage
		// like an enter vehicle, followed by a flee, when the damage event is received we'll compute an in vehicle death clip
		// if the flee task gets the ped to exit, we'll end up using the in vehicle death anim outside of the vehicle.
		SetState(State_Finish);
	}
	
	return FSM_Continue;
}

void	CTaskUseSequence::Finish_OnEnter()
{
	// reset the GLOBALFLAG_SCRIPT_MIGRATION flag on the ped to what it was before the task started
	if (m_bPedCanMigrate && GetPed() && GetPed()->GetNetworkObject())
	{
		GetPed()->GetNetworkObject()->SetGlobalFlag(CNetObjGame::GLOBALFLAG_SCRIPT_MIGRATION, false);
	}
}

void CTaskUseSequence::InitialiseSequenceSubtask(aiTask* pCloneTask) const
{
#if !__FINAL
	if (GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_USE_SEQUENCE &&
		pCloneTask->GetTaskType() == CTaskTypes::TASK_USE_SEQUENCE )
	{
		aiAssertf(0, "Sequences of sequences of sequences not allowed!");
	}
#endif

	if (m_iSubtaskSequenceProgress != -1)
	{
		if (pCloneTask->GetTaskType() == CTaskTypes::TASK_USE_SEQUENCE)
		{
			CTaskUseSequence* pTaskUseSequence = static_cast<CTaskUseSequence*>(pCloneTask);
			pTaskUseSequence->SetProgress(m_iSubtaskSequenceProgress,-1);
		}
	}
}

aiTask* CTaskUseSequence::CreateTask()
{
	aiAssert( m_iPrimarySequenceProgress <= CTaskList::MAX_LIST_SIZE);

	aiTask* pNextTask = NULL;

	const CTaskSequenceList* pTaskList = GetTaskSequenceList();

	taskFatalAssertf(pTaskList, "Task sequence list was null in CTaskUseSequence::CreateTask");

	// We're not repeating the sequence
	if (pTaskList->GetRepeatMode() == CTaskSequenceList::REPEAT_NOT)
	{
		// If we've reached the end of the task list or there is no task at the current slot return NULL as we've finished
		if ((m_iPrimarySequenceProgress == CTaskList::MAX_LIST_SIZE) || (!pTaskList->GetTask(m_iPrimarySequenceProgress)))
		{
			return NULL;
		}
		else
		{
			// Return next task in sequence.
			aiAssert(pTaskList->GetTask(m_iPrimarySequenceProgress));
			pNextTask = pTaskList->GetTask(m_iPrimarySequenceProgress)->Copy();
			aiAssert(pNextTask->GetTaskType() == pTaskList->GetTask(m_iPrimarySequenceProgress)->GetTaskType());
		}
	}
	else // m_iRepeatMode == REPEAT_FOREVER
	{
		// If we've reached the end of the task list or there is no task at the current slot reset the sequence to start again
		if ((CTaskList::MAX_LIST_SIZE == m_iPrimarySequenceProgress) || (!pTaskList->GetTask(m_iPrimarySequenceProgress)))
		{
			m_iPrimarySequenceProgress = 0;		// Reset the sequence progress
			++m_iRepeatProgress;				 // Increment number of completed sequence loops.
		}	

		// Return next task in sequence.
		aiAssert(pTaskList->GetTask(m_iPrimarySequenceProgress));
		pNextTask = pTaskList->GetTask(m_iPrimarySequenceProgress)->Copy();
		aiAssert(pNextTask->GetTaskType() == pTaskList->GetTask(m_iPrimarySequenceProgress)->GetTaskType());
	}

	return pNextTask;
}

//-------------------------------------------------------------------------
// Calculate any subtask [progress, returns -1 if no progress
//-------------------------------------------------------------------------
s32 CTaskUseSequence::GetSubtaskProgress() const
{
	if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_USE_SEQUENCE)
	{
		const CTaskUseSequence* pTaskUseSequence = static_cast<const CTaskUseSequence*>(GetSubTask());
		return pTaskUseSequence->GetPrimarySequenceProgress();
	}
	return -1;
}

const CTaskSequenceList* CTaskUseSequence::GetTaskSequenceList() const 
{ 
	const CTaskSequenceList* pTaskList = GetStaticTaskSequenceList();
	if (!pTaskList)
	{
		return GetDynamicTaskSequenceList();
	}
	return pTaskList;
}

const CTaskSequenceList* CTaskUseSequence::GetStaticTaskSequenceList() const 
{ 
	if (GetSequenceID() != -1)
	{
		taskAssert(!m_pTaskSequenceList);
		return &CTaskSequences::ms_TaskSequenceLists[GetSequenceID()]; 
	}
	return NULL;
}

const CTaskSequenceList* CTaskUseSequence::GetDynamicTaskSequenceList() const 
{ 
	return m_pTaskSequenceList;
}

CTaskInfo* CTaskUseSequence::CreateQueriableState() const
{
	return rage_new CClonedTaskSequenceInfo(GetSequenceID(),
											m_sequenceResourceId,
											GetPrimarySequenceProgress(),
											GetSubtaskSequenceProgress(),
											GetExitAfterInterrupted());
}

void  CTaskUseSequence::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	taskAssert(pTaskInfo);
	taskAssert(pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_SEQUENCE);

	CClonedTaskSequenceInfo* taskSequenceInfo = static_cast< CClonedTaskSequenceInfo* >(pTaskInfo);

	m_iPrimarySequenceProgress  = taskSequenceInfo->GetPrimarySequenceProgress();
	m_iSubtaskSequenceProgress  = taskSequenceInfo->GetSubtaskSequenceProgress();
	m_bExitAfterInterrupted     = taskSequenceInfo->GetExitAfterInterrupted();

	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}


#if !__FINAL
const char * CTaskUseSequence::GetStateName( s32 iState ) const
{
	if (iState == State_RunningTask )
	{
		static char szReturnString[64];
		formatf(szReturnString, "State_RunningTask : Progress = %i", m_iPrimarySequenceProgress); 
		return szReturnString;

	}

	return CTaskFSMClone::GetStateName( iState );
}

const char * CTaskUseSequence::GetStaticStateName( s32 iState )
{
	static const char *stateName[] = 
	{
		"State_Start",
		"State_RunningTask",
		"State_Finish"
	};
	return stateName[iState];
}
#endif // !__FINAL

///////////////
//Play Open Door clip, depending on mood (if player)
///////////////
const fwMvClipSetId CTaskOpenDoor::ms_UnarmedClipSetId( "DOORS@UNARMED",0xA9AF0B5C );
const fwMvClipSetId CTaskOpenDoor::ms_1HandedClipSetId( "DOORS@1HANDED",0xF60C4DE );
const fwMvClipSetId CTaskOpenDoor::ms_2HandedClipSetId( "DOORS@2HANDED",0x311AB202 );
const fwMvClipSetId CTaskOpenDoor::ms_2HandedMeleeClipSetId( "DOORS@2HMelee",0x6AB9A8AC );
const fwMvClipId CTaskOpenDoor::ms_AnimClipId( "AnimClip_Out",0x11C1C8B0 );
const fwMvFloatId CTaskOpenDoor::ms_ActorDirectionId( "ActorDirection_In",0x3BD416FB );
const fwMvFloatId CTaskOpenDoor::ms_AnimClipRateId( "AnimClipRate_In",0xAF34220E );
const fwMvFloatId CTaskOpenDoor::ms_AnimClipPhaseId( "AnimClipPhase_Out",0x65FEFAE9 );
const fwMvFlagId CTaskOpenDoor::ms_ActorUseLeftArmId( "ActorUseLeft",0xDBDE9D22 );
const fwMvBooleanId CTaskOpenDoor::ms_BargeAnimFinishedId( "OnAnimFinishedBarge",0x6E4FF703 );
const fwMvFilterId CTaskOpenDoor::ms_RightArmFilterId( "RightArmFilter_In",0x29A5C6C2 );
const fwMvFilterId CTaskOpenDoor::ms_LeftArmFilterId( "LeftArmFilter_In",0xE54DAB86 );
dev_float CTaskOpenDoor::ms_fDirectionBlendRate = 5.0f;
dev_float CTaskOpenDoor::ms_fMaxActorToDoorReachingDistance = 0.5f;
dev_float CTaskOpenDoor::ms_fMinTimeBeforeIkUse = 0.25f;
dev_float CTaskOpenDoor::ms_fMaxActorToDoorDistanceRunning = 2.0f;
dev_float CTaskOpenDoor::ms_fMaxActorToDoorDistance = 1.41f;
dev_float CTaskOpenDoor::ms_fMinActorToDoorDistance = 0.50f;
dev_float CTaskOpenDoor::ms_fWalkingDoorConstraintOffset = 0.4f;
dev_float CTaskOpenDoor::ms_fRunningDoorConstraintOffset = 0.45f;
dev_float CTaskOpenDoor::ms_fConstraintGrowthRate = 2.0f;
dev_float CTaskOpenDoor::ms_fPedDotToDoorThresholdRunning = -0.6f;
dev_float CTaskOpenDoor::ms_fPedDotToDoorThreshold = -0.26f;
dev_float CTaskOpenDoor::ms_fMaxBargeAnimRate = 2.5f;
dev_float CTaskOpenDoor::ms_fMBRWalkBoundary = 0.75f;
dev_u32	CTaskOpenDoor::ms_nBargeDoorDelay = 500;

CTaskOpenDoor::CTaskOpenDoor( CDoor* pDoor )
: m_pDoor( pDoor )
, m_bUseLeftArm( true )
, m_vClosestDoorPosition( V_ZERO )
, m_fActorDirection( 0.0f )
, m_fConstraintDistance( 0.25f )	// set to the default ped capsule radius
, m_pLeftArmFilter( NULL )
, m_pRightArmFilter( NULL )
, m_fTargetAnimRate( 1.0f )
, m_fCriticalFramePhase( -1.0f )
, m_bPastCriticalFrame( false )
, m_bSetOpenDoorArm( false )
, m_bSetActorDirection( false )
, m_bInFrontOfDoor( false )
, m_bInsideDoorWidth( false )
, m_bInsideDoorHeight( false )
, m_bAppliedBargeImpulse( true )
{
	SetInternalTaskType(CTaskTypes::TASK_OPEN_DOOR);
}

CTask::FSM_Return CTaskOpenDoor::ProcessPreFSM()
{
	// End the task if the ped starts to aim
	CPed* pPed = GetPed();
	if( !pPed || (!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_OpenDoorArmIK ) && !pPed->GetPedResetFlag( CPED_RESET_FLAG_OpenDoorArmIK )) || !m_pDoor )
		return FSM_Quit;

	if (pPed->GetIKSettings().IsIKFlagDisabled(IKManagerSolverTypes::ikSolverTypeArm))
	{
		return FSM_Quit;
	}

	m_bInFrontOfDoor = IsPedInFrontOfDoor( pPed, m_pDoor );
	m_vClosestDoorPosition = CalculateClosestDoorPosition( pPed, m_pDoor, pPed->GetTransform().GetPosition(), m_bInFrontOfDoor, m_bInsideDoorWidth, m_bInsideDoorHeight );
	if( !m_bInsideDoorWidth || !m_bInsideDoorHeight )
		return FSM_Quit;

#if __BANK
	CTask::ms_debugDraw.AddSphere( m_vClosestDoorPosition, 0.05f, m_bInFrontOfDoor ? Color_green : Color_red, 2, atStringHash( "OPEN_DOOR_CLOSEST_POSITION" ), false );
	CTask::ms_debugDraw.AddAxis( m_pDoor->GetMatrix(), 1.0f, true, 2, atStringHash( "OPEN_DOOR_AXIS" ) );

	// Display flat dist to door
	Vec3V vPedToClosestDoorPosition = m_vClosestDoorPosition - pPed->GetTransform().GetPosition();
	float fPedToDoorDistance = Mag( vPedToClosestDoorPosition ).Getf();
	char szTemp[128];
	formatf( szTemp, 128, "%f\n", fPedToDoorDistance );
	CTask::ms_debugDraw.AddText( m_vClosestDoorPosition, 0, 0, szTemp, Color_white, 2, atStringHash( "OPEN_DOOR_CLOSEST_POSITION_FLAT_DIST" ) );
#endif

	// Update which arm we should use (1st frame is FREE)
	if( !m_bSetOpenDoorArm )
	{
		m_bUseLeftArm = ShouldUseLeftHand( pPed, m_pDoor, m_vClosestDoorPosition, m_bInFrontOfDoor ); 
		m_bSetOpenDoorArm = true;
	}
	else if( GetState() == State_OpenDoor )
	{
		// Did we switch arms?
		if( m_bUseLeftArm != ShouldUseLeftHand( pPed, m_pDoor, m_vClosestDoorPosition, m_bInFrontOfDoor ) )
			return FSM_Quit;

		// Are we trying to use an arm we don't allow?
		if( ( pPed->GetPedResetFlag( CPED_RESET_FLAG_OnlyAllowLeftArmDoorIk ) && !m_bUseLeftArm ) ||
			( pPed->GetPedResetFlag( CPED_RESET_FLAG_OnlyAllowRightArmDoorIk ) && m_bUseLeftArm ) )
			return FSM_Quit;
	}

	return FSM_Continue;
}

//update the local state
CTask::FSM_Return CTaskOpenDoor::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CPed *pPed = GetPed();
	FSM_Begin

		FSM_State(State_Initial)
			FSM_OnUpdate
				return StateInitial_OnUpdate( pPed );

		FSM_State(State_OpenDoor)
			FSM_OnEnter
				return StateOpenDoor_OnEnter( pPed );
			FSM_OnUpdate
				return StateOpenDoor_OnUpdate( pPed );

		FSM_State(State_ConstraintOnly)
			FSM_OnUpdate
				return StateConstraintOnly_OnUpdate( pPed );

		FSM_State(State_Barge)
			FSM_OnEnter
				return StateBarge_OnEnter( pPed );
			FSM_OnUpdate
				return StateBarge_OnUpdate( pPed );
			FSM_OnExit
				return StateBarge_OnExit( pPed );

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End	
}

void CTaskOpenDoor::CleanUp()
{
	CPed *pPed = GetPed();
	if ( pPed )
	{
		pPed->GetMovePed().ClearSecondaryTaskNetwork( m_moveNetworkHelper, GetIsInsideDoorWidth() ? SLOW_BLEND_DURATION : REALLY_SLOW_BLEND_DURATION );
	}

	if( m_PhysicsConstraint.IsValid() )
	{
		PHCONSTRAINT->Remove( m_PhysicsConstraint );
		m_PhysicsConstraint.Reset();
	}

	if( m_pLeftArmFilter )
	{
		m_pLeftArmFilter->Release();
		m_pLeftArmFilter = NULL;
	}

	if( m_pRightArmFilter )
	{
		m_pRightArmFilter->Release();
		m_pRightArmFilter = NULL;
	}

	if( m_pDoor )
	{
		m_pDoor->SetPlayerDoorState( CDoor::PDS_NONE );
	}

	m_pDoor = NULL;
}

void CTaskOpenDoor::ProcessClipEvents( const CPed* pPed )
{
	if( m_moveNetworkHelper.IsNetworkActive() )
	{
		if( m_fCriticalFramePhase == -1.0f )
		{
			const crClip* pClip = m_moveNetworkHelper.GetClip( ms_AnimClipId );
			if( pClip )
			{
				// Mark it as initialized 
				m_fCriticalFramePhase = 0.0f;
				const fwMvClipSetId clipsetId = m_moveNetworkHelper.GetClipSetId();
				if( clipsetId != CLIP_SET_ID_INVALID )
				{
					fwClipSet* pClipSet = fwClipSetManager::GetClipSet( clipsetId );
					if( pClipSet )
					{
						CClipEventTags::FindEventPhase( pClip, CClipEventTags::CriticalFrame, m_fCriticalFramePhase);

						// let the system know that we can properly apply a barge impulse
						m_bAppliedBargeImpulse = false;
					}
				}

				// We only care if we haven't already passed the target interrupt phase
				float fTimeRemaining = m_fCriticalFramePhase - m_moveNetworkHelper.GetFloat( ms_AnimClipPhaseId );
				if( fTimeRemaining > 0.0f )
				{
					// Determine how long it will take to get to the desired location with the current velocity
					float fVelocity = pPed->GetVelocity().Mag();
					if( fVelocity > 0.0f )
					{
						// Initialize the anim rate to as fast as possible
						m_fTargetAnimRate = ms_fMaxBargeAnimRate;

						Vec3V vPedPosition = pPed->GetTransform().GetPosition();
						Vec3V vClosestDoorPosition = GetClosestDoorPosition();

						// Adjust the y axis to best fit the sweeping clip
						AdjustHeight( pPed, vPedPosition, vClosestDoorPosition );

						float fDistToClosestPoint = Mag( vClosestDoorPosition - vPedPosition ).Getf();
						float fDistanceRemaining = MAX( fDistToClosestPoint - ms_fRunningDoorConstraintOffset, 0.0f );
						if( fDistanceRemaining > 0.0f )
						{
							float fTimeUntilArrival = fDistanceRemaining / fVelocity;
							m_fTargetAnimRate = MIN( fTimeRemaining / fTimeUntilArrival, ms_fMaxBargeAnimRate );
						}					
					}
				}
			}
		}

		float fCurrentPhase = m_moveNetworkHelper.GetFloat( ms_AnimClipPhaseId );
		if( fCurrentPhase > m_fCriticalFramePhase )
			m_bPastCriticalFrame = true;
	}
}

CTask::FSM_Return CTaskOpenDoor::StateInitial_OnUpdate( CPed* pPed )
{
	if( PrepareMoveNetwork( pPed ) )
	{
		// Set up the hand constraint
		if( !m_PhysicsConstraint.IsValid() )
		{
			phConstraintHalfSpace::Params halfSpaceConstraint; 
			halfSpaceConstraint.instanceA = pPed->GetCurrentPhysicsInst(); 
			halfSpaceConstraint.instanceB = m_pDoor->GetCurrentPhysicsInst(); 
			halfSpaceConstraint.worldAnchorA = pPed->GetTransform().GetPosition();
			halfSpaceConstraint.worldAnchorB = m_pDoor->GetTransform().GetPosition();
			halfSpaceConstraint.worldNormal = m_pDoor->GetTransform().GetB();
			m_PhysicsConstraint = PHCONSTRAINT->Insert( halfSpaceConstraint );
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskOpenDoor::StateOpenDoor_OnEnter( CPed* pPed )
{
	m_moveNetworkHelper.SendRequest( fwMvRequestId( "OnEnterOpenDoor",0xE98F58FE ) );
	
	if( !m_pLeftArmFilter )
	{
		static const atHashWithStringNotFinal sLeftArmFilterId( "UpperbodyFeathered_NoRightArm_filter",0x7320B763 );
		m_pLeftArmFilter = CGtaAnimManager::FindFrameFilter( sLeftArmFilterId.GetHash(), pPed );
		if( m_pLeftArmFilter )
			m_pLeftArmFilter->AddRef();
	}

	if( !m_pRightArmFilter )
	{
		static const atHashWithStringNotFinal sRightArmFilterId( "UpperbodyFeathered_NoLefttArm_filter",0xEFBF5E79 );
		m_pRightArmFilter = CGtaAnimManager::FindFrameFilter( sRightArmFilterId.GetHash(), pPed );
		if( m_pRightArmFilter )
		{
			m_pRightArmFilter->AddRef();
		}
	}
		
	return FSM_Continue;
}

CTask::FSM_Return CTaskOpenDoor::StateOpenDoor_OnUpdate( CPed* pPed )
{
	if( pPed->IsUsingActionMode() || pPed->GetMotionData()->GetUsingStealth() )
		return FSM_Quit;

	// Are we carrying a 2 handed weapon?
	const CWeaponInfo* pWeaponInfo = GetPedWeaponInfo(pPed);
	if( !pWeaponInfo || pWeaponInfo->GetIsTwoHanded() )
		return FSM_Quit;

	const float fCurrentMBRSq = pPed->GetMotionData()->GetCurrentMoveBlendRatio().Mag2();

	// Quit out if are running and delay the barge for a bit
	if( fCurrentMBRSq > rage::square( CArmIkSolver::GetDoorBargeMoveBlendThreshold() ) )
	{
		pPed->SetLastTimeWeBargedThroughDoor( fwTimer::GetTimeInMilliseconds() );
		return FSM_Quit;
	}
	
	Vec3V vPedPosition = pPed->GetTransform().GetPosition();
	Vec3V vClosestDoorPosition = GetClosestDoorPosition();
	Vec3V vPedToClosestPos =  vClosestDoorPosition - vPedPosition;
	
	float fPedDotToDoor = CalculatePedDotToDoor( pPed, m_pDoor, GetIsInFrontOfDoor() );
	if( fPedDotToDoor > ms_fPedDotToDoorThreshold )
		return FSM_Quit;
	
	float fActorDistanceToDoorSq = MagXYSquared( vPedToClosestPos ).Getf();

	// Are we outside the distance threshold?
	float fDistanceThreshold = Lerp( Abs( fPedDotToDoor ), ms_fMinActorToDoorDistance, ms_fMaxActorToDoorDistance );
	if( fActorDistanceToDoorSq > rage::square( fDistanceThreshold ) )
		return FSM_Quit;

	// Are we standing still and outside the door space?
	if( fCurrentMBRSq < rage::square( ms_fMBRWalkBoundary ) && fActorDistanceToDoorSq > rage::square( ms_fMaxActorToDoorReachingDistance ) )
		return FSM_Quit;

	if( NavigatingPedMustAbort( pPed, m_pDoor ) )
		return FSM_Quit;

	// This produces an arm glitch otherwise
	CTaskMotionBase* pTask = pPed->GetCurrentMotionTask();
	if( pTask && pTask->GetTaskType() == CTaskTypes::TASK_HUMAN_LOCOMOTION && static_cast<CTaskHumanLocomotion*>(pTask)->GetState() == CTaskHumanLocomotion::State_Turn180 )
		return FSM_Quit;
	
	// Adjust the y axis to best fit the sweeping clip
	AdjustHeight( pPed, vPedPosition, vClosestDoorPosition );

	bool bIsInFrontOfDoor = GetIsInFrontOfDoor();
	UpdateConstraint( pPed, bIsInFrontOfDoor, vPedPosition, vClosestDoorPosition, ms_fWalkingDoorConstraintOffset );
	ApplyCloseDelay( pPed, bIsInFrontOfDoor );

	// Do not start the ik until a certain amount of time spent in this state
	if( GetTimeInState() > rage::square( ms_fMinTimeBeforeIkUse ) )
	{
		Vec3V vHandBone( V_ZERO );
		bool bHandValid = pPed->GetBonePositionVec3V( vHandBone, m_bUseLeftArm ? BONETAG_L_HAND : BONETAG_R_HAND );

#if FPS_MODE_SUPPORTED
		CIkRequestArm ikRequest( m_bUseLeftArm ? IK_ARM_LEFT : IK_ARM_RIGHT, NULL, BONETAG_INVALID, vClosestDoorPosition, AIK_TARGET_WRT_IKHELPER );

		ikRequest.SetBlendInRate( ARMIK_BLEND_RATE_NORMAL );
		ikRequest.SetBlendOutRate( ARMIK_BLEND_RATE_FAST );

		if( pPed->IsFirstPersonShooterModeEnabledForPlayer() )
		{
			TUNE_GROUP_FLOAT( PED_DOOR, FirstPersonAngle, 35.0f, -180.0f, 180.0f, 0.1f );

			Mat34V mtxEntity( pPed->GetTransform().GetMatrix() );
			Vec3V vTargetPosition( vClosestDoorPosition );
			Vec3V vNeckBone( V_ZERO );
			Vec3V vIkHelperBone( V_ZERO );

			const bool bNeckValid = pPed->GetBonePositionVec3V( vNeckBone, BONETAG_NECK );
			const bool bIkHelperValid = pPed->GetBonePositionVec3V( vIkHelperBone, m_bUseLeftArm ? BONETAG_L_IK_HAND : BONETAG_R_IK_HAND );

			if( bHandValid && bNeckValid && bIkHelperValid )
			{
				// Adjust target position by ik helper offset
				vTargetPosition = Add( vTargetPosition, Subtract( vHandBone, vIkHelperBone ) );

				// Rotate hand to account for increased height of hand in first person
				Vec3V vDoorToNeck( Subtract( vNeckBone, vTargetPosition ) );
				vDoorToNeck = Normalize( vDoorToNeck );
				vDoorToNeck = UnTransform3x3Full( mtxEntity, vDoorToNeck );

				ScalarV vAngle( FirstPersonAngle * DtoR );
				vAngle = SelectFT( VecBoolV( BoolV( m_bUseLeftArm ) ), Negate( vAngle ), vAngle );

				ikRequest.SetAdditive( QuatVFromAxisAngle( vDoorToNeck, vAngle ) );
			}

			ikRequest.SetTargetOffset( vTargetPosition );
			ikRequest.SetFlags( AIK_USE_ORIENTATION | AIK_USE_TWIST_CORRECTION );
			ikRequest.SetBlendInRate( ARMIK_BLEND_RATE_FAST );
		}

		pPed->GetIkManager().Request( ikRequest );
#else
		pPed->GetIkManager().SetAbsoluteArmIKTarget( m_bUseLeftArm ? crIKSolverArms::LEFT_ARM : crIKSolverArms::RIGHT_ARM, VEC3V_TO_VECTOR3( vClosestDoorPosition ), AIK_TARGET_WRT_IKHELPER );
#endif // FPS_MODE_SUPPORTED

		if(m_pDoor && m_pDoor->GetDoorAudioEntity() && bHandValid && MagSquared( vHandBone - vClosestDoorPosition ).Getf() < audDoorAudioEntity::sm_PedHandPushDistance )
		{
			m_pDoor->GetDoorAudioEntity()->EntityWantsToOpenDoor( VEC3V_TO_VECTOR3( vClosestDoorPosition ), pPed->GetVelocity().Mag() );
			m_pDoor->GetDoorAudioEntity()->TriggerDoorImpact(pPed, vClosestDoorPosition, true);
		}	
	}

	// Send over MoVE params
	SendParams( pPed );
	return FSM_Continue;
}

CTask::FSM_Return CTaskOpenDoor::StateConstraintOnly_OnUpdate( CPed* pPed )
{
	// Stay in this state if we are in action/stealth mode or carrying a 2 handed weapon
	const CWeaponInfo* pWeaponInfo = GetPedWeaponInfo(pPed);
	bool bActionOrStealthMode = pPed->IsUsingActionMode() || pPed->GetMotionData()->GetUsingStealth();
	if( !bActionOrStealthMode && !( pWeaponInfo && pWeaponInfo->GetIsTwoHanded() ) )
		return FSM_Quit;

	Vec3V vPedPosition = pPed->GetTransform().GetPosition();
	Vec3V vClosestDoorPosition = GetClosestDoorPosition();
	Vec3V vPedToClosestPos =  vClosestDoorPosition - vPedPosition;

	float fPedDotToDoor = CalculatePedDotToDoor( pPed, m_pDoor, GetIsInFrontOfDoor() );
	if( fPedDotToDoor > ms_fPedDotToDoorThreshold )
		return FSM_Quit;

	float fActorDistanceToDoorSq = MagXYSquared( vPedToClosestPos ).Getf();

	// Are we outside the distance threshold?
	float fDistanceThreshold = Lerp( Abs( fPedDotToDoor ), ms_fMinActorToDoorDistance, ms_fMaxActorToDoorDistance );
	if( fActorDistanceToDoorSq > rage::square( fDistanceThreshold ) )
		return FSM_Quit;

	if( NavigatingPedMustAbort( pPed, m_pDoor ) )
		return FSM_Quit;

	// Adjust the y axis to best fit the sweeping clip
	AdjustHeight( pPed, vPedPosition, vClosestDoorPosition );

	bool bIsInFrontOfDoor = GetIsInFrontOfDoor();
	UpdateConstraint( pPed, bIsInFrontOfDoor, vPedPosition, vClosestDoorPosition, ms_fRunningDoorConstraintOffset );
	ApplyCloseDelay( pPed, bIsInFrontOfDoor );
	return FSM_Continue;
}

CTask::FSM_Return CTaskOpenDoor::StateBarge_OnEnter( CPed* )
{
	m_moveNetworkHelper.SendRequest( fwMvRequestId( "OnEnterBarge", 0xB6C73D62 ) );
	return FSM_Continue;
}

CTask::FSM_Return CTaskOpenDoor::StateBarge_OnUpdate( CPed* pPed )
{
	// Terminate task once the anim has finished
	if( m_moveNetworkHelper.IsNetworkActive() && m_moveNetworkHelper.GetBoolean( ms_BargeAnimFinishedId ) )
		return FSM_Quit;
	
	if( pPed->IsUsingActionMode() || pPed->GetMotionData()->GetUsingStealth() )
		return FSM_Quit;

	// Quit out if we are anything slower than our decided barge ratio
	bool bRunning = pPed->GetMotionData()->GetCurrentMoveBlendRatio().Mag2() > rage::square( CArmIkSolver::GetDoorBargeMoveBlendThreshold() );
	if( !bRunning )
		return FSM_Quit;

	Vec3V vPedPosition = pPed->GetTransform().GetPosition();
	Vec3V vClosestDoorPosition = GetClosestDoorPosition();

	float fPedDotToDoor = CalculatePedDotToDoor( pPed, m_pDoor, GetIsInFrontOfDoor() );
	if( fPedDotToDoor > ms_fPedDotToDoorThresholdRunning )
		return FSM_Quit;

	Vec3V vHandBone( V_ZERO );
	bool bGotHandPos = pPed->GetBonePositionVec3V( vHandBone, BONETAG_L_HAND);
	if( m_pDoor && m_pDoor->GetDoorAudioEntity() && bGotHandPos && MagSquared( vHandBone - vClosestDoorPosition ).Getf() < audDoorAudioEntity::sm_PedHandPushDistance )
	{
		m_pDoor->GetDoorAudioEntity()->EntityWantsToOpenDoor( VEC3V_TO_VECTOR3( vClosestDoorPosition ), pPed->GetVelocity().Mag() );
		m_pDoor->GetDoorAudioEntity()->TriggerDoorImpact(pPed, vClosestDoorPosition, true);
	}
	
	// Adjust the y axis to best fit the sweeping clip
	AdjustHeight( pPed, vPedPosition, vClosestDoorPosition );

	ProcessClipEvents( pPed );

	bool bIsInFrontOfDoor = GetIsInFrontOfDoor();
	if( m_pDoor && m_bPastCriticalFrame && !m_bAppliedBargeImpulse )
	{
		m_bAppliedBargeImpulse = true;

		float fImpulseDirection = bIsInFrontOfDoor ? -1.0f : 1.0f;
		float fImpulseForce = pPed->GetMass();
		Vec3V vHitImpulse = Scale( m_pDoor->GetTransform().GetB(), ScalarV( fImpulseDirection * fImpulseForce ) );

		Vec3V vHingeVector = m_pDoor->GetTransform().GetA(); 	
		if( Abs( m_pDoor->GetBoundingBoxMin().x ) > Abs( m_pDoor->GetBoundingBoxMax().x ) )
		{
			vHingeVector = Negate( vHingeVector );
			vHitImpulse = Negate( vHitImpulse );
		}

		float fDoorWidth = Abs( m_pDoor->GetBoundingBoxMax().x - m_pDoor->GetBoundingBoxMin().x );
		Vec3V vDoorEdge = Scale( vHingeVector, ScalarV( fDoorWidth ) );
		m_pDoor->ApplyImpulse( VEC3V_TO_VECTOR3( vHitImpulse ), VEC3V_TO_VECTOR3( vDoorEdge ), 0 );
	}

	UpdateConstraint( pPed, bIsInFrontOfDoor, vPedPosition, vClosestDoorPosition, ms_fWalkingDoorConstraintOffset );
	ApplyCloseDelay( pPed, bIsInFrontOfDoor );		

	// Send over MoVE params
	SendParams( pPed );
	return FSM_Continue;
}

CTask::FSM_Return CTaskOpenDoor::StateBarge_OnExit( CPed* pPed )
{
	static dev_float sfMinTimeInStateToApplyDelay = 0.25f;
	if( GetTimeInState() > sfMinTimeInStateToApplyDelay )
		pPed->SetLastTimeWeBargedThroughDoor( fwTimer::GetTimeInMilliseconds() );

	return FSM_Continue;
}

void CTaskOpenDoor::ApplyCloseDelay( CPed* pPed, bool bInFront ) 
{
	if (!NetworkInterface::IsGameInProgress() && pPed->IsPlayer() )
	{
		// Let the door know we have hit it
		m_pDoor->SetPlayerDoorState( bInFront ? CDoor::PDS_FRONT : CDoor::PDS_BACK );
		if( !m_pDoor->GetTuning() || !m_pDoor->GetTuning()->m_Flags.IsFlagSet( CDoorTuning::DontCloseWhenTouched ) )
		{
			float fOpenDoorRatio = m_pDoor->GetDoorOpenRatio();
			if( (  bInFront && fOpenDoorRatio >  0.01f  ) || 
				( !bInFront && fOpenDoorRatio < -0.01f  ) )
			{
				m_pDoor->SetTargetDoorRatio( 0.0f, false );
				m_pDoor->SetCloseDelay( 0.0f );
			}
		}
		
		if( !m_pDoor->GetDoorControlFlag( CDoor::DOOR_CONTROL_FLAGS_STUCK ) )
		{
			bool bCurrentlyMoving = false;
			CTaskMotionBase* pTask = pPed->GetCurrentMotionTask();
			if( pTask && pTask->GetTaskType() == CTaskTypes::TASK_HUMAN_LOCOMOTION )
			{
				if( static_cast<CTaskHumanLocomotion*>(pTask)->GetState() == CTaskHumanLocomotion::State_Moving )
					bCurrentlyMoving = true;
			}
			else
				bCurrentlyMoving = pPed->GetMotionData()->GetCurrentMoveBlendRatio().Mag2() > rage::square( ms_fMBRWalkBoundary );

			if( bCurrentlyMoving && m_pDoor->GetTuning() && m_pDoor->GetTuning()->m_Flags.IsFlagSet( CDoorTuning::DelayDoorClosingForPlayer ) )
			{
				// If the door is not 15% open then don't worry about it yet. This little bit is to 
				// prevent the door from opening before the ped has actually started trying to open the door with the constraint.
				static float fMinOpenDoorRatio = 0.15f;
				float fOpenDoorRatio = m_pDoor->GetDoorOpenRatio();
				if( (  bInFront && fOpenDoorRatio < -fMinOpenDoorRatio ) || 
					( !bInFront && fOpenDoorRatio >  fMinOpenDoorRatio ) )
				{
					m_pDoor->SetTargetDoorRatio( bInFront ? -1.0f : 1.0f, false );
					m_pDoor->SetCloseDelay( CDoor::ms_fPlayerDoorCloseDelay );
				}
			}
		}
	}
}

const CWeaponInfo* CTaskOpenDoor::GetPedWeaponInfo(const CPed* pPed) const
{
	const CPedWeaponManager* pWeaponMgr = pPed->GetWeaponManager();
	if(pWeaponMgr)
	{
		const CObject* pWeaponObject = pWeaponMgr->GetEquippedWeaponObject();
		if( !pWeaponObject || !pWeaponObject->GetIsVisible() )
		{
			const CWeapon* pUnarmedWeapon = pWeaponMgr->GetUnarmedEquippedWeapon();
			if(pUnarmedWeapon)
				return pUnarmedWeapon->GetWeaponInfo();
		}

		return pWeaponMgr->GetEquippedWeaponInfo();
	}

	return NULL;
}

// Initialize the move network associated with the OpenDoor task
bool CTaskOpenDoor::PrepareMoveNetwork( CPed* pPed )
{
	const CWeaponInfo* pEquippedWeaponInfo = GetPedWeaponInfo(pPed);
	if( !pEquippedWeaponInfo )
	{
		SetState( State_Finish );
		return false;
	}

	if( pPed->IsUsingActionMode() || pPed->GetMotionData()->GetUsingStealth() )
	{
		SetState( State_ConstraintOnly );
		return true;
	}

	// Are we walking and carrying a 2 handed weapon?
	bool bRunning = pPed->GetMotionData()->GetCurrentMoveBlendRatio().Mag2() >= rage::square( CArmIkSolver::GetDoorBargeMoveBlendThreshold() );
	const CObject *pWeaponObject = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponObject() : NULL;
	if( !bRunning && pEquippedWeaponInfo->GetIsTwoHanded() && pWeaponObject)
	{
		SetState( State_ConstraintOnly );
		return true;
	}

	// Wait until the network worker is streamed in and initialized 
	if ( !m_moveNetworkHelper.IsNetworkActive() )
	{		
		m_moveNetworkHelper.RequestNetworkDef( CClipNetworkMoveInfo::ms_NetworkTaskOnFootOpenDoor );

		fwMvClipSetId weaponClipSet;
		if( pEquippedWeaponInfo->GetIsUnarmed() )
			weaponClipSet = ms_UnarmedClipSetId;
		else if( pEquippedWeaponInfo->GetIsTwoHanded() )
			weaponClipSet = pEquippedWeaponInfo->GetIsMelee() ? ms_2HandedMeleeClipSetId : ms_2HandedClipSetId;
		else
			weaponClipSet = ms_1HandedClipSetId;
		
		fwClipSet* pSet = fwClipSetManager::GetClipSet( weaponClipSet );
		taskAssert(pSet);

		if( pSet->Request_DEPRECATED() && m_moveNetworkHelper.IsNetworkDefStreamedIn( CClipNetworkMoveInfo::ms_NetworkTaskOnFootOpenDoor ) )
		{
			m_moveNetworkHelper.CreateNetworkPlayer( pPed, CClipNetworkMoveInfo::ms_NetworkTaskOnFootOpenDoor );
			pPed->GetMovePed().SetSecondaryTaskNetwork( m_moveNetworkHelper.GetNetworkPlayer(), bRunning ? NORMAL_BLEND_DURATION : REALLY_SLOW_BLEND_DURATION );
			m_moveNetworkHelper.SetClipSet( weaponClipSet );

			if( bRunning )
				SetState( State_Barge );
			else
				SetState( State_OpenDoor );

			// Success
			return true;
		}
	}

	// Waiting
	return false;
}

void CTaskOpenDoor::UpdateConstraint( CPed* pPed, bool bFront, Vec3V_In vPedPosition, Vec3V_In vClosestPosition, float fConstraintOffset )
{
	if( m_PhysicsConstraint.IsValid() )
	{
		Vec3V vTempVector = vPedPosition;
		phConstraintHalfSpace* pConstraint = static_cast<phConstraintHalfSpace*>( PHCONSTRAINT->GetTemporaryPointer( m_PhysicsConstraint ) );
		if( pConstraint )
		{
			phArchetype * pPhysArch = pPed->GetPhysArch();
			if( pPhysArch )
			{
				phBound* pBound = pPhysArch->GetBound();
				if( pBound )
				{
					phBoundComposite* pBoundComposite = static_cast<phBoundComposite*>(pPhysArch->GetBound());
					Mat34V capsuleMat( pBoundComposite->GetCurrentMatrix( 0 ) );
					vTempVector += capsuleMat.GetCol3();
				}
			}

			Approach( m_fConstraintDistance, fConstraintOffset, ms_fConstraintGrowthRate, fwTimer::GetTimeStep() );
			Vec3V vPositionOffset( V_ZERO );
			vPositionOffset.SetYf( m_fConstraintDistance );

			Vec3V vPedToClosest = vClosestPosition - vTempVector;
			float fBaseHeadingRads = rage::Atan2f( -vPedToClosest.GetXf(), vPedToClosest.GetYf() );

			vPositionOffset = RotateAboutZAxis( vPositionOffset, ScalarV( fwAngle::LimitRadianAngle( fBaseHeadingRads ) ) );
			vPositionOffset += vTempVector;	

			Vec3V vDoorZCOM = vClosestPosition;
			if ( phInst* pDoorInst = pConstraint->GetInstanceB ( ) )
			{
				Vec3V vDoorCOM = pDoorInst->GetCenterOfMass( );
				vDoorZCOM.SetZ( vDoorCOM.GetZ() );
			}

			static dev_float sfRatioEpsilon = 0.02f;
			bool bIsFullyOpen = m_pDoor ? m_pDoor->IsDoorFullyOpen( sfRatioEpsilon ) : false;
			if( bIsFullyOpen )
			{
				pConstraint->SetMassInvScaleB( 0.0f );
			}
			else
			{
				pConstraint->SetMassInvScaleB( 1.0f );
			}

			pConstraint->SetWorldPosA( vPositionOffset );
			pConstraint->SetWorldPosB( vDoorZCOM );
			pConstraint->SetWorldNormal( CalculateConstraintNormal( pPed, m_pDoor, bFront ) );

#if __BANK
			CTask::ms_debugDraw.AddSphere( pConstraint->GetWorldPosA(), 0.05f, Color_blue, 2, atStringHash( "OPEN_DOOR_CONSTRAINT_POSITION" ) );
			CTask::ms_debugDraw.AddArrow( pConstraint->GetWorldPosA(), pConstraint->GetWorldPosB(), 0.25f, Color_blue, 2, atStringHash( "OPEN_DOOR_CONSTRAINT_DIRECTION" ) );
#endif
		}
	}
}

// Determine what side of the door the ped is on 
bool CTaskOpenDoor::IsPedInFrontOfDoor( const CPed* pPed, CDoor* pDoor )
{
	taskAssert( pPed );
	taskAssert( pDoor );

	Mat34V matDoor = pDoor->GetMatrix();
	Vec3V vDelta = pPed->GetTransform().GetPosition() - matDoor.GetCol3();

	// Check to see if the door is inverted in model space and if so, invert the forward vector
	Vec3V vDoorVector = matDoor.GetCol1(); 
	if( pDoor->GetBoundingBoxMin().x > -pPed->GetCapsuleInfo()->GetHalfWidth() )
		vDoorVector = Negate( vDoorVector ); 

	return Dot( vDelta, vDoorVector ).Getf() < 0.0f;
}

bool CTaskOpenDoor::ShouldUseLeftHand( const CPed* pPed, CDoor* pDoor, Vec3V_In, bool bFront )
{
	static dev_float sfArmThresholdMin = 0.525f;
	static dev_float sfArmThresholdMax = 0.725f;

	taskAssert( pPed );
	taskAssert( pDoor );
	bool bStill = ( pPed->GetMotionData()->GetCurrentMoveBlendRatio().Mag2() < rage::square( ms_fMBRWalkBoundary ) );
	
	// Normalized between 0.0f and 1.0f since MoVE curves do not allow for negative numbers
	Mat34V matDoor = pDoor->GetMatrix();

	// Check to see if the door is inverted in model space and if so, invert the hinge vector
	Vec3V vHingeVector = matDoor.GetCol0(); 
	if( pDoor->GetBoundingBoxMin().x > -pPed->GetCapsuleInfo()->GetHalfWidth() )
		vHingeVector = Negate( vHingeVector ); 

	float fHingeDirection = ( Dot( vHingeVector, pPed->GetTransform().GetB() ).Getf() + 1.0f ) * 0.5f;

	float fLeftArmThreshold = bStill ? sfArmThresholdMin : Lerp( Abs( pDoor->GetDoorOpenRatio() ), sfArmThresholdMax, sfArmThresholdMin );
	if( fHingeDirection < fLeftArmThreshold )
		return !bFront;
	
	return bFront;
}

float CTaskOpenDoor::CalculatePedDotToDoor( const CPed* pPed, CDoor* pDoor, bool bInFrontOfDoor )
{
	taskAssert( pPed );
	taskAssert( pDoor );

	Mat34V matDoor = pDoor->GetMatrix();
	Vec3V vDelta;

	// Invert the perp vector depending on the side and whether the door is inverted in model space
	if( pDoor->GetBoundingBoxMin().x > -pPed->GetCapsuleInfo()->GetHalfWidth() )
		vDelta = bInFrontOfDoor ? matDoor.GetCol1() : Negate( matDoor.GetCol1() );
	else
		vDelta = bInFrontOfDoor ? Negate( matDoor.GetCol1() ) : matDoor.GetCol1();

	return Dot( vDelta, pPed->GetTransform().GetB() ).Getf();
}

// Calculate the door normal that will be used for the hand to door physics constraint
Vec3V_Out CTaskOpenDoor::CalculateConstraintNormal( const CPed* pPed, CDoor* pDoor, bool bFront )
{
	taskAssert( pPed );
	taskAssert( pDoor );
	Vec3V vConstraintNormal( V_ZERO );

	// Invert the perp vector depending on the side and whether the door is inverted in model space
	if( pDoor->GetBoundingBoxMin().x > -pPed->GetCapsuleInfo()->GetHalfWidth() )
		vConstraintNormal = bFront ? pDoor->GetTransform().GetB() : -pDoor->GetTransform().GetB();
	else
		vConstraintNormal = bFront ? -pDoor->GetTransform().GetB() : pDoor->GetTransform().GetB();

	return vConstraintNormal;
}

// Defines the quiting conditions of the task for external use
bool CTaskOpenDoor::ShouldAllowTask( const CPed* pPed, CDoor* pDoor )
{
	const CPedIKSettingsInfo& ikInfo = const_cast<CPed*>(pPed)->GetIKSettings();
	if (ikInfo.IsIKFlagDisabled(IKManagerSolverTypes::ikSolverTypeArm))
	{
		return false;
	}

	const CTask* pTask = pPed->GetPedIntelligence()->FindTaskActiveByTreeAndType( PED_TASK_TREE_SECONDARY, CTaskTypes::TASK_MOBILE_PHONE );
	if( pTask && pTask->GetState() != CTaskMobilePhone::State_Finish )
		return false;

	// Make sure the door is unhinged
	int nDoorType = CDoor::DOOR_TYPE_NOT_A_DOOR;
	bool bIsLocked = true;
	if( pDoor )
	{
		nDoorType = pDoor->GetDoorType();
		bIsLocked = pDoor->IsBaseFlagSet( fwEntity::IS_FIXED ); 
	}
	
	if( ( nDoorType != CDoor::DOOR_TYPE_STD && nDoorType != CDoor::DOOR_TYPE_STD_SC ) || bIsLocked )
		return false;

	// Are we walking and carrying a 2 handed weapon?
	const float fCurrentMBRSq = pPed->GetMotionData()->GetCurrentMoveBlendRatio().Mag2();
	bool bRunning = fCurrentMBRSq >= rage::square( CArmIkSolver::GetDoorBargeMoveBlendThreshold() );

	// Check against the barge door delay if we are running fast enough
	u32 nLastTimeWeBargedThroughDoor = pPed->GetLastTimeWeBargedThroughDoor();
	if( bRunning && nLastTimeWeBargedThroughDoor != 0 && (( fwTimer::GetTimeInMilliseconds() - nLastTimeWeBargedThroughDoor) < ms_nBargeDoorDelay ) )
		return false;

	// Is the designated ped facing the door?
	bool bFront = IsPedInFrontOfDoor( pPed, pDoor );

	// Are we inside the door constraints?
	bool bInsideDoorWidth = false, bInsideDoorHeight = false;
	Vec3V vPedPosition = pPed->GetTransform().GetPosition();
	Vec3V vClosestPosition = CalculateClosestDoorPosition( pPed, pDoor, vPedPosition, bFront, bInsideDoorWidth, bInsideDoorHeight );
	if( !bInsideDoorWidth || !bInsideDoorHeight )
		return false;

	Vec3V vPedToClosestPos =  vClosestPosition - vPedPosition;
	float fPedDotToDoor = CalculatePedDotToDoor( pPed, pDoor, bFront );
	bool bShouldUseLeftHand = ShouldUseLeftHand( pPed, pDoor, vClosestPosition, bFront );
	float fPedDotToDoorThreshold = bRunning ? ms_fPedDotToDoorThresholdRunning : ms_fPedDotToDoorThreshold;
	if( fPedDotToDoor > fPedDotToDoorThreshold )
		return false;

	float fActorDistanceToDoorSq = MagXYSquared( vPedToClosestPos ).Getf();
	
	// Are we outside the distance threshold?
	float fDistanceThreshold = Lerp( Abs( fPedDotToDoor ), ms_fMinActorToDoorDistance, ms_fMaxActorToDoorDistance );
	if( fActorDistanceToDoorSq > rage::square( fDistanceThreshold ) )
		return false;

	// Are we standing still and outside the door space?
	if( fActorDistanceToDoorSq > rage::square( ms_fMaxActorToDoorReachingDistance ) )
	{
		if( fCurrentMBRSq < rage::square( ms_fMBRWalkBoundary ) )
			return false;
		else if( !pPed->GetPedResetFlag( CPED_RESET_FLAG_AllowOpenDoorIkBeforeFullMovement ) )
		{
			// Grab the locomotion task
			CTaskMotionBase* pTask = pPed->GetCurrentMotionTask();
			if( pTask && pTask->GetTaskType() == CTaskTypes::TASK_HUMAN_LOCOMOTION )
			{
				// If we are in any other state beside moving... don't run the task
				if( static_cast<CTaskHumanLocomotion*>(pTask)->GetState() != CTaskHumanLocomotion::State_Moving )
					return false;
			}
		}
	}

	if( ( pPed->GetPedResetFlag( CPED_RESET_FLAG_OnlyAllowLeftArmDoorIk ) && !bShouldUseLeftHand ) ||
		( pPed->GetPedResetFlag( CPED_RESET_FLAG_OnlyAllowRightArmDoorIk ) && bShouldUseLeftHand ) )
		return false;

	if( NavigatingPedMustAbort(pPed, pDoor) )
		return false;

	return true;
}

// Find the point on the door closest to the ped and figure out if we're not between the hinge and door end anymore
Vec3V_Out CTaskOpenDoor::CalculateClosestDoorPosition( const CPed* pPed, CDoor* pDoor, Vec3V_In vTestPosition, bool bFront, bool &bInsideDoorWidth, bool &bInsideDoorHeight )
{
	Mat34V rMat = pDoor->GetMatrix();

	// Check to see if the door's right vector points away from it's width, if so negate the hinge vector
	Vec3V vHingeVector = rMat.GetCol0(); 
	Vec3V vDoorHingePos( V_ZERO ); 
	if( pDoor->GetBoundingBoxMin().x < -pPed->GetCapsuleInfo()->GetHalfWidth() )
	{
		vHingeVector = Negate( vHingeVector ); 
		vDoorHingePos.SetYf( bFront ? pDoor->GetBoundingBoxMin().y : -pDoor->GetBoundingBoxMin().y );
	}
	else
		vDoorHingePos.SetYf( bFront ? -pDoor->GetBoundingBoxMin().y : pDoor->GetBoundingBoxMin().y );

	vDoorHingePos = RotateAboutZAxis( vDoorHingePos, ScalarV( pDoor->GetTransform().GetHeading() ) );
	vDoorHingePos += rMat.GetCol3();	

	float fWidth = Abs( pDoor->GetBoundingBoxMax().x - pDoor->GetBoundingBoxMin().x );
	Vec3V vHingeToDoorEdge = Scale( vHingeVector, ScalarV( fWidth ) );

	//A simple distance check isn't good enough since we can have some extra wide doors. So we need to do the distance
	//check from the ped to the nearest point on the door's width.
	Vec3V vClosestPoint = geomPoints::FindClosestPointSegToPoint( vDoorHingePos, vHingeToDoorEdge, vTestPosition );

	// figure out if we're inside the edge and hinge
	bool bOnInsideOfDoorEdge = false;
	bool bOnInsideOfHinge = IsTrue( IsGreaterThan( Dot( vHingeVector, vTestPosition ), Dot( vHingeVector, vDoorHingePos ) ) );
	if( bOnInsideOfHinge )
	{
		float fDoorEdgeScale = 1.0f;
		if( !pDoor->GetTuning() || !pDoor->GetTuning()->m_Flags.IsFlagSet( CDoorTuning::IgnoreOpenDoorTaskEdgeLerp ) )
		{
			// Slightly push the door edge out to compensate for double doors
			static dev_float sfDoorEdgeScaleMin = 1.0f;
			static dev_float sfDoorEdgeScaleMax = 1.25f;
			fDoorEdgeScale = Lerp( Abs( pDoor->GetDoorOpenRatio() ), sfDoorEdgeScaleMax, sfDoorEdgeScaleMin );
		}

		Vec3V vDoorEdgePos = rMat.GetCol3() + Scale( vHingeToDoorEdge, ScalarV( fDoorEdgeScale ) );
		bOnInsideOfDoorEdge = IsTrue( IsLessThan( Dot( vHingeVector, vTestPosition ), Dot( vHingeVector, vDoorEdgePos ) ) );
	}
	bInsideDoorWidth = bOnInsideOfHinge && bOnInsideOfDoorEdge;

	// NOTE: assumes the hinge position is close to the mid point between the z height min/max
	float fHeight = Abs( pDoor->GetBoundingBoxMax().z - pDoor->GetBoundingBoxMin().z ) * 0.5f;
	float fPlayerZ = pPed->GetTransform().GetPosition().GetZf();
	float fPlayerHeight = pPed->GetCapsuleInfo() ? pPed->GetCapsuleInfo()->GetGroundToRootOffset() : 0.0f;
	bInsideDoorHeight = ( fPlayerZ < ( vDoorHingePos.GetZf() + fHeight ) && ( fPlayerZ + fPlayerHeight ) > ( vDoorHingePos.GetZf() - fHeight ) );
	
	return vClosestPoint;
}


bool CTaskOpenDoor::NavigatingPedMustAbort( const CPed* pPed, CDoor* pDoor )
{
	if(pPed->GetPedResetFlag(CPED_RESET_FLAG_IgnoreNavigationForDoorArmIK))
	{
		return false;
	}

	// So we can walk around entity instead
	CTaskMove * pSimplestMoveTask = pPed->GetPedIntelligence()->GetActiveSimplestMovementTask();
	if(CPedIntelligence::ms_bPedsRespondToObjectCollisionEvents && pSimplestMoveTask && pSimplestMoveTask->GetMoveInterface()->HasTarget() && (pSimplestMoveTask->IsTaskMoving()) )
	{
		CTaskMoveInterface* pTaskMove = pSimplestMoveTask->GetMoveInterface();
		const Vector3 & vGoToTarget = pTaskMove->GetTarget();

		if(pTaskMove && pTaskMove->HasTarget())
		{
			const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			CEntityBoundAI bound(*pDoor, vPedPosition.z, pPed->GetCapsuleInfo()->GetHalfWidth());
			const int iPedSide = bound.ComputeHitSideByPosition(vPedPosition);
			const int iTargetSide = bound.ComputeHitSideByPosition(vGoToTarget);
			if(iPedSide != iTargetSide)
			{
				Vector3 vMoveDir = vGoToTarget - vPedPosition;
				vMoveDir.Normalize();
				if (pDoor->GetDoorIsAtLimit(vPedPosition, vMoveDir))
					return true;
			}
		}
	}

	return false;
}

// Sends useful parameters to the associated MoVE network
void CTaskOpenDoor::SendParams( CPed* pPed )
{	
	taskAssert( m_moveNetworkHelper.IsNetworkActive() );

	Mat34V matDoor = m_pDoor->GetMatrix();
	Vec3V vHingeVector = matDoor.GetCol0(); 
	if( m_pDoor->GetBoundingBoxMin().x > -pPed->GetCapsuleInfo()->GetHalfWidth() )
		vHingeVector = Negate( vHingeVector );

	// Normalized between 0.0f and 1.0f since MoVE curves do not allow for negative numbers
	float fTargetDirection = Dot( vHingeVector, pPed->GetTransform().GetB() ).Getf();

	// Crazy logic but doors are modeled in different ways
	bool bShouldUseLeftArm = GetShouldUseLeftArm();
	if( !GetIsInFrontOfDoor() )
	{
		if( bShouldUseLeftArm && fTargetDirection > 0.0f )
			fTargetDirection = 0.0f;
		else if( fTargetDirection < 0.0f )
			fTargetDirection = Abs( fTargetDirection );
	}
	// Back of door
	else
	{
		if( !bShouldUseLeftArm && fTargetDirection > 0.0f )
			fTargetDirection = 0.0f;
		else if( fTargetDirection < 0.0f )
			fTargetDirection = Abs( fTargetDirection );
	}

	// Force the actor direction to match the target direction
	if( !m_bSetActorDirection )
	{
		m_fActorDirection = fTargetDirection;
		m_bSetActorDirection = true;
	}
	else
		Approach( m_fActorDirection, fTargetDirection, ms_fDirectionBlendRate, fwTimer::GetTimeStep() );

	m_moveNetworkHelper.SetFlag( bShouldUseLeftArm, ms_ActorUseLeftArmId );
	m_moveNetworkHelper.SetFloat( ms_ActorDirectionId, m_fActorDirection );

	if( bShouldUseLeftArm )
		m_moveNetworkHelper.SetFilter( m_pLeftArmFilter, ms_LeftArmFilterId );
	else
		m_moveNetworkHelper.SetFilter( m_pRightArmFilter, ms_RightArmFilterId );

	// If we found an anim interrupt event calculate the anim rate to achieve that 
	float fTimeRemaining = m_fCriticalFramePhase - m_moveNetworkHelper.GetFloat( ms_AnimClipPhaseId );
	if( fTimeRemaining > 0 )
		m_moveNetworkHelper.SetFloat( ms_AnimClipRateId, m_fTargetAnimRate );
	else
		m_moveNetworkHelper.SetFloat( ms_AnimClipRateId, 1.0f );
}

void CTaskOpenDoor::AdjustHeight( const CPed* pPed, Vec3V& vPedPosition, Vec3V& vDoorPosition )
{
	Vec3V vReferenceBone( V_ZERO );
	eAnimBoneTag referenceBoneTag = BONETAG_SPINE3;

#if FPS_MODE_SUPPORTED
	if( pPed->IsFirstPersonShooterModeEnabledForPlayer() )
	{
		referenceBoneTag = BONETAG_NECK;
	}
#endif // FPS_MODE_SUPPORTED

	if( pPed->GetBonePositionVec3V( vReferenceBone, referenceBoneTag ) )
	{
		vDoorPosition.SetZf( vReferenceBone.GetZf() );
		vPedPosition.SetZf( vReferenceBone.GetZf() );
	}
}

#if !__FINAL

// return the name of the given task as a string

const char * CTaskOpenDoor::GetStaticStateName( s32 iState  )
{	
	// Make sure FSM state is valid
	taskAssert( iState <= State_Barge );
	switch( iState )
	{
		case State_Initial: return "Initial";
		case State_OpenDoor: return "OpenDoor";
		case State_ConstraintOnly: return "ConstraintOnly";
		case State_Barge: return "Barge";
		case State_Finish: return "Finish";
		default: { Assert( false ); return "Unknown"; }
	}
}
#endif //!__FINAL

////////////////////
// Say audio
////////////////////

CTaskSayAudio::CTaskSayAudio( const char* szContext, float fDelayBeforeAudio, float fDelayAfterAudio, CEntity* pFaceTowardEntity,u32 szContextReply, float fHeadingRateMultiplier )
:	m_bIsSpeaking(false),
m_fDelayBeforeAudio(fDelayBeforeAudio),
m_fDelayAfterAudio(fDelayAfterAudio),
m_pFaceTowardEntity(pFaceTowardEntity),
m_fHeadingRateMultiplier(fHeadingRateMultiplier),
m_AudioReplyHash(szContextReply)
{
	Assert( strlen(szContext) < (AUDIO_NAME_LENGTH - 1) );
	strcpy(m_szAudioName, szContext);
	SetInternalTaskType(CTaskTypes::TASK_SAY_AUDIO);
}

CTaskSayAudio::~CTaskSayAudio()
{
}

CTask::FSM_Return CTaskSayAudio::UpdateFSM(const s32 iState, const FSM_Event iEvent )
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		// Entrance state for the task.
		FSM_State(State_Start)
		FSM_OnUpdate
		Start_OnUpdate(pPed);

	// Delay before starting audio.
	FSM_State(State_WaitingToSayAudio)
		FSM_OnUpdate
		return WaitingToSayAudio_OnUpdate(pPed);

	// Start the audio playing.
	FSM_State(State_SayAudio)
		FSM_OnUpdate
		return SayAudio_OnUpdate(pPed);

	// The audio is playing.
	FSM_State(State_SpeakingAudio)
		FSM_OnEnter
		SpeakingAudio_OnEnter(pPed);
	FSM_OnUpdate
		return SpeakingAudio_OnUpdate(pPed);
	FSM_OnExit
		SpeakingAudio_OnExit(pPed);

	// Delay before moving to State_Finish.
	FSM_State(State_WaitingToFinish)
		FSM_OnUpdate
		return WaitingToFinish_OnUpdate(pPed);

	// Quit
	FSM_State(State_Finish)
		FSM_OnUpdate
		return FSM_Quit;
	FSM_End
}

// Helper functions for FSM update above:

CTask::FSM_Return CTaskSayAudio::Start_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	SetState(State_WaitingToSayAudio);

	return FSM_Continue;
}

CTask::FSM_Return CTaskSayAudio::WaitingToSayAudio_OnUpdate(CPed* pPed)
{
	// Decrement the delay counter or, if the delay has already elapsed, switch to the SayAudio state.

	if( m_fDelayBeforeAudio <= 0.0f )
	{
		SetState(State_SayAudio);
	}
	else
	{
		m_fDelayBeforeAudio -= fwTimer::GetTimeStep();
	}

	MakePedFaceTowardEntity(pPed);

	return FSM_Continue;
}

CTask::FSM_Return CTaskSayAudio::SayAudio_OnUpdate(CPed* pPed)
{
	// Start the ped speaking the audio file.

	//pPed->NewSay(m_AudioHash);
	if (m_pFaceTowardEntity && m_pFaceTowardEntity->GetIsTypePed() && m_AudioReplyHash != 0)
	{
		CPed* pReplyingPed = static_cast<CPed*>(m_pFaceTowardEntity.Get());
		pPed->NewSay(m_szAudioName, 0, true, false,-1,pReplyingPed,m_AudioReplyHash);
	}
	else
	{
		pPed->NewSay(m_szAudioName, 0, true);
	}

	SetState(State_SpeakingAudio);
	MakePedFaceTowardEntity(pPed);

	return FSM_Continue;
}

void CTaskSayAudio::SpeakingAudio_OnEnter(CPed* pPed)
{
	// Set a boolean flag for MakeAbortableImpl().
	m_bIsSpeaking = true;

	MakePedFaceTowardEntity(pPed);
}

CTask::FSM_Return CTaskSayAudio::SpeakingAudio_OnUpdate(CPed* pPed)
{
	// Ped is currently speaking. Check to see if audio is finished yet and switch to WaitingToFinish state if it is.

	if( !pPed->GetSpeechAudioEntity() || !pPed->GetSpeechAudioEntity()->IsAmbientSpeechPlaying() )
	{
		SetState(State_WaitingToFinish);
	}

	return FSM_Continue;
}

void CTaskSayAudio::SpeakingAudio_OnExit(CPed* UNUSED_PARAM(pPed))
{
	// Unset a boolean flag for MakeAbortableImpl().
	m_bIsSpeaking = false;
}

CTask::FSM_Return CTaskSayAudio::WaitingToFinish_OnUpdate(CPed* pPed)
{
	// Decrement the delay counter or, if the delay has already elapsed, switch to the finish state.

	if( m_fDelayAfterAudio <= 0.0f )
	{
		SetState(State_Finish);
	}
	else
	{
		m_fDelayAfterAudio -= fwTimer::GetTimeStep();
	}

	MakePedFaceTowardEntity(pPed);

	return FSM_Continue;
}

void CTaskSayAudio::MakePedFaceTowardEntity(CPed* pPed)
{
	// Face the ped towards the entity.
	if( m_pFaceTowardEntity )
	{
		const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		const Vector3 vFaceTowardPosition = VEC3V_TO_VECTOR3(m_pFaceTowardEntity->GetTransform().GetPosition());
		float fDesiredHeading=fwAngle::GetRadianAngleBetweenPoints(vFaceTowardPosition.x, vFaceTowardPosition.y, vPedPosition.x, vPedPosition.y);
		fDesiredHeading=fwAngle::LimitRadianAngle(fDesiredHeading);
		pPed->SetDesiredHeading(fDesiredHeading);
		pPed->SetHeadingChangeRate( CPedType::ms_fMaxHeadingChange * m_fHeadingRateMultiplier );
		if( m_pFaceTowardEntity->GetIsTypePed() )
		{
			pPed->GetIkManager().LookAt(0, m_pFaceTowardEntity, 100, BONETAG_HEAD, NULL );
		}
	}
}

#if !__FINAL
const char * CTaskSayAudio::GetStaticStateName( s32 iState )
{
	Assert(iState>=State_Start&&iState<=State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_WaitingToSayAudio",
		"State_SayAudio",
		"State_SpeakingAudio",
		"State_WaitingToFinish",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

void CTaskSayAudio::CleanUp()
{
	if( GetPed()->GetSpeechAudioEntity() && GetPed()->GetSpeechAudioEntity()->IsAmbientSpeechPlaying() && m_bIsSpeaking)
	{
		GetPed()->GetSpeechAudioEntity()->StopSpeech();
	}

	GetPed()->RestoreHeadingChangeRate();
}

// End of CTaskSayAudio.
////////////////////////////////////////////////////////////////////////////////////////////////////////

void CTaskComplexUseMobilePhoneAndMovement::CleanUp()
{
    // Clear the vehicle task running on the vehicle task manager
    CPed* pPed = GetPed(); //Get the ped ptr.
    if( pPed && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) )
    {
        CVehicle* pVehicle = pPed->GetMyVehicle();
        if( pVehicle && pVehicle->IsDriver(pPed) )
        {
            pVehicle->GetIntelligence()->GetTaskManager()->ClearTask(VEHICLE_TASK_TREE_PRIMARY, VEHICLE_TASK_PRIORITY_PRIMARY);
        }
    }
}

aiTask* CTaskComplexUseMobilePhoneAndMovement::CreateFirstSubTask( CPed* pPed )
{
	if( pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) )
	{
        //set the player drive task on the vehicle
        CVehicle* pVehicle = pPed->GetMyVehicle();

        if(pVehicle && pVehicle->IsDriver(pPed) && pPed->IsPlayer())//only create a vehicle driving task if the ped is a player and the driver.
        {
            pVehicle->SetPlayerDriveTask(pPed);
        }

		return rage_new CTaskComplexUseMobilePhone(m_iTime);
	}
	else
	{
		return rage_new CTaskComplexControlMovement( rage_new CTaskMoveStandStill(), rage_new CTaskComplexUseMobilePhone(m_iTime), CTaskComplexControlMovement::TerminateOnSubtask );
	}
}

CTaskFSMClone *CClonedSetBlockingOfNonTemporaryEventsInfo::CreateCloneFSMTask()
{
	return rage_new CTaskSetBlockingOfNonTemporaryEvents(m_bBlock);
}

CTask *CClonedSetBlockingOfNonTemporaryEventsInfo::CreateLocalTask(fwEntity *UNUSED_PARAM(pEntity))
{
	return CreateCloneFSMTask();
}

void CTaskSetBlockingOfNonTemporaryEvents::SetCharState()
{
	if( taskVerifyf(GetPed(), "CTaskSetBlockingOfNonTemporaryEvents::Null Ped!") )
	{
		GetPed()->SetBlockingOfNonTemporaryEvents(m_bBlock);
	}
}

CTaskInfo* CTaskSetBlockingOfNonTemporaryEvents::CreateQueriableState() const
{
	return rage_new CClonedSetBlockingOfNonTemporaryEventsInfo(m_bBlock);
}

CTaskFSMClone* CTaskSetBlockingOfNonTemporaryEvents::CreateTaskForClonePed(CPed* UNUSED_PARAM(pPed))
{
	return rage_new CTaskSetBlockingOfNonTemporaryEvents(m_bBlock);
}

CTaskFSMClone *CClonedForceMotionStateInfo::CreateCloneFSMTask()
{
	return rage_new CTaskForceMotionState((CPedMotionStates::eMotionState)m_State, m_bForceRestart);
}

void CTaskForceMotionState::SetCharState()
{
	if( taskVerifyf(GetPed(), "CTaskForceMotionState::Null Ped!") )
	{
		if( taskVerifyf(CPed::IsAllowedToForceMotionState(m_State, GetPed()), "CTaskForceMotionState - Failed to force state %d - See TTY for details", m_State) )
		{
			GetPed()->ForceMotionStateThisFrame(m_State, m_bForceRestart);
		}
	}
}

CTaskInfo* CTaskForceMotionState::CreateQueriableState() const
{
	return rage_new CClonedForceMotionStateInfo(m_State, m_bForceRestart);
}

CTaskFSMClone* CTaskForceMotionState::CreateTaskForClonePed(CPed* UNUSED_PARAM(pPed))
{
	return rage_new CTaskForceMotionState(m_State, m_bForceRestart);
}

const fwMvClipSetId CTaskShovePed::ms_UnarmedShoveClipSetId( "ANIM_GROUP_STD_PED",0x7e8db8c6 );
const fwMvClipSetId CTaskShovePed::ms_UnarmedClipSetId( "DOORS@UNARMED",0xA9AF0B5C );
const fwMvClipSetId CTaskShovePed::ms_1HandedClipSetId( "DOORS@1HANDED",0xF60C4DE );
const fwMvClipSetId CTaskShovePed::ms_2HandedClipSetId( "DOORS@2HANDED",0x311AB202 );
const fwMvClipId CTaskShovePed::ms_AnimClipId( "AnimClip_Out",0x11C1C8B0 );
const fwMvFloatId CTaskShovePed::ms_ActorDirectionId( "ActorDirection_In",0x3BD416FB );
const fwMvFloatId CTaskShovePed::ms_AnimClipRateId( "AnimClipRate_In",0xAF34220E );
const fwMvFloatId CTaskShovePed::ms_AnimClipPhaseId( "AnimClipPhase_Out",0x65FEFAE9 );
const fwMvFlagId CTaskShovePed::ms_ActorUseLeftArmId( "ActorUseLeft",0xDBDE9D22 );
const fwMvBooleanId CTaskShovePed::ms_BargeAnimFinishedId( "OnAnimFinishedBarge",0x6E4FF703 );
const fwMvFilterId CTaskShovePed::ms_RightArmFilterId( "RightArmFilter_In",0x29A5C6C2 );
const fwMvFilterId CTaskShovePed::ms_LeftArmFilterId( "LeftArmFilter_In",0xE54DAB86 );
dev_float CTaskShovePed::ms_fDirectionBlendRate = 5.0f;
dev_float CTaskShovePed::ms_fMinTimeBeforeIkUse = 0.075f;
dev_float CTaskShovePed::ms_fMaxActorToTargetDistanceRunning = 1.5f;
dev_float CTaskShovePed::ms_fActorToTargetHeightThreshold = 0.25f;
dev_float CTaskShovePed::ms_fPedTestHeightOffset = 0.25f;
dev_float CTaskShovePed::ms_fMaxActorToTargetDistance = 1.41f;
dev_float CTaskShovePed::ms_fMinActorToTargetDistance = 0.50f;
dev_float CTaskShovePed::ms_fRunningTargetOffset = 0.45f;
dev_float CTaskShovePed::ms_fPedDotToTargetThresholdRunning = 0.3f;
dev_float CTaskShovePed::ms_fPedDotToTargetThreshold = 0.0f;
dev_float CTaskShovePed::ms_fMaxBargeAnimRate = 2.5f;
dev_float CTaskShovePed::ms_fMinPedVelocityToBarge = 2.0f;

bool CTaskShovePed::ScanForShoves()
{
	TUNE_GROUP_BOOL(PED_SHOVING, bScanForShoves, true);
	return bScanForShoves;
}

#if __BANK
bool CTaskShovePed::IsTesting()
{
	TUNE_GROUP_BOOL(PED_SHOVING, bTestPedShoving, false)
	return bTestPedShoving;
}
#endif

CTaskShovePed::CTaskShovePed( CPed* pTargetPed )
: m_pTargetPed( pTargetPed )
, m_bUseLeftArm( false )
, m_fActorDirection( 0.0f )
, m_pLeftArmFilter( NULL )
, m_pRightArmFilter( NULL )
, m_fTargetAnimRate( 1.0f )
, m_fInterruptPhase( 0.0f )
, m_bComputeTargetAnimRate( false )
, m_bDoTargetReaction(false)
, m_fBlendOutDuration( REALLY_SLOW_BLEND_DURATION )
{
	SetInternalTaskType(CTaskTypes::TASK_SHOVE_PED);
}

CTask::FSM_Return CTaskShovePed::ProcessPreFSM()
{
	CPed *pPed = GetPed();
	if( !m_pTargetPed || pPed->GetMotionData()->GetIsStill())
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskShovePed::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CPed *pPed = GetPed();
	FSM_Begin

	FSM_State(State_Initial)
		FSM_OnUpdate
			return StateInitial_OnUpdate( pPed );

	FSM_State(State_Shove)
		FSM_OnEnter
			return StateShove_OnEnter( pPed );
		FSM_OnUpdate
			return StateShove_OnUpdate( pPed );

	FSM_State(State_Push)
			FSM_OnEnter
			return StatePush_OnEnter( pPed );
		FSM_OnUpdate
			return StatePush_OnUpdate( pPed );

	FSM_State(State_Barge)
		FSM_OnEnter
			return StateBarge_OnEnter( pPed );
		FSM_OnUpdate
			return StateBarge_OnUpdate( pPed );

	FSM_State(State_Finish)
		FSM_OnUpdate
			return FSM_Quit;

	FSM_End	
}

void CTaskShovePed::CleanUp()
{
	CPed *pPed = GetPed();
	if ( pPed )
	{
		pPed->GetMovePed().ClearSecondaryTaskNetwork(m_moveNetworkHelper, GetBlendOutDuration() );
	}

	if( m_pLeftArmFilter )
	{
		m_pLeftArmFilter->Release();
		m_pLeftArmFilter = NULL;
	}

	if( m_pRightArmFilter )
	{
		m_pRightArmFilter->Release();
		m_pRightArmFilter = NULL;
	}
}

bool CTaskShovePed::ShouldAllowTask( const CPed* pPed, CPed* pTargetPed )
{
	if(pPed->GetPedResetFlag(CPED_RESET_FLAG_UsingMobilePhone))
	{
		return false;
	}

	if(!pPed->CanShovePed(pTargetPed))
	{
		return false;
	}

	CPedIntelligence* pPedIntelligence = pPed->GetPedIntelligence();
	if( pPedIntelligence && pPedIntelligence->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_MELEE ) )
	{
		return false;
	}

	bool bRunning = (pPed->GetMotionData()->GetIsRunning() || pPed->GetMotionData()->GetIsSprinting()) && pPed->GetVelocity().Mag2() > rage::square(ms_fMinPedVelocityToBarge);
	
	TUNE_GROUP_BOOL(PED_SHOVING, bDoPushAnim, false);
	if(!bRunning && !bDoPushAnim)
	{
		return false;
	}

	// Are we walking and carrying a 2 handed weapon?
	const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponInfo() : NULL;
	if( !pWeaponInfo || ( !bRunning && pWeaponInfo->GetIsTwoHanded() ) )
	{
		return false;
	}

	float fPedToTargetDot = CalculatePedDotToTargetPed(pPed, pTargetPed);
	float fPedDotToDoorThreshold = bRunning ? ms_fPedDotToTargetThresholdRunning : ms_fPedDotToTargetThreshold;
	if( fPedToTargetDot < fPedDotToDoorThreshold )
	{
		return false;
	}

	// Are we near enough to target.
	Vector3 vClosestPosition = CalculateClosestTargetPedPosition(pPed, pTargetPed);
	Vector3 vPedPos = VEC3V_TO_VECTOR3( pPed->GetTransform().GetPosition() );
	vPedPos.z += ms_fPedTestHeightOffset;
	Vector3 vPedToClosestPos =  vClosestPosition - vPedPos;
	float fActorDistanceToTargetSq = vPedToClosestPos.XYMag2();

	// Are we outside the distance threshold?
	float fDistanceThreshold = Lerp( Abs( fPedToTargetDot ), ms_fMinActorToTargetDistance, bRunning ? ms_fMaxActorToTargetDistanceRunning : ms_fMaxActorToTargetDistance );
	if( Abs( vPedToClosestPos.z ) > ms_fActorToTargetHeightThreshold || fActorDistanceToTargetSq > rage::square( fDistanceThreshold ) )
	{
		return false;
	}

	return true;
}

CTask::FSM_Return CTaskShovePed::StateInitial_OnUpdate( CPed* pPed )
{
	PrepareMoveNetwork( pPed );
	return FSM_Continue;
}

CTask::FSM_Return CTaskShovePed::StatePush_OnEnter( CPed* pPed )
{
	m_moveNetworkHelper.SendRequest( fwMvRequestId( "OnEnterPushDoor" ) );

	if( !m_pLeftArmFilter )
	{
		static const atHashWithStringNotFinal sLeftArmFilterId( "UpperbodyFeathered_NoRightArm_filter" );
		m_pLeftArmFilter = CGtaAnimManager::FindFrameFilter( sLeftArmFilterId.GetHash(), pPed );
		if( m_pLeftArmFilter )
			m_pLeftArmFilter->AddRef();
	}

	if( !m_pRightArmFilter )
	{
		static const atHashWithStringNotFinal sRightArmFilterId( "UpperbodyFeathered_NoLefttArm_filter" );
		m_pRightArmFilter = CGtaAnimManager::FindFrameFilter( sRightArmFilterId.GetHash(), pPed );
		if( m_pRightArmFilter )
		{
			m_pRightArmFilter->AddRef();
		}
	}

	// Force send MoVE params
	SendParams( pPed, true, true );

	return FSM_Continue;
}

CTask::FSM_Return CTaskShovePed::StatePush_OnUpdate( CPed* pPed )
{
	// Are we carrying a 2 handed weapon?
	const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponInfo() : NULL;
	if( !pWeaponInfo || pWeaponInfo->GetIsTwoHanded() )
	{
		return FSM_Quit;
	}

	// Is the designated ped facing the target?
	float fPedToTargetDot = CalculatePedDotToTargetPed(pPed, m_pTargetPed);
	if( fPedToTargetDot < ms_fPedDotToTargetThreshold )
	{
		return FSM_Quit;
	}

	// Are we near enough to target.
	Vector3 vClosestPosition = CalculateClosestTargetPedPosition(pPed, m_pTargetPed);
	Vector3 vPedPos = VEC3V_TO_VECTOR3( pPed->GetTransform().GetPosition() );
	vPedPos.z += ms_fPedTestHeightOffset;
	Vector3 vPedToClosestPos =  vClosestPosition - vPedPos;
	float fActorDistanceToTargetSq = vPedToClosestPos.XYMag2();

	// Are we outside the distance threshold?
	float fDistanceThreshold = Lerp( Abs( fPedToTargetDot ), ms_fMinActorToTargetDistance, ms_fMaxActorToTargetDistance );
	if( Abs( vPedToClosestPos.z ) > ms_fActorToTargetHeightThreshold || fActorDistanceToTargetSq > rage::square( fDistanceThreshold ) )
	{
		return FSM_Quit;
	}

	// Send over MoVE params
	if( !SendParams( pPed, false, true ) )
	{
		// If we changed arms then quit out of the current instance
		return FSM_Quit;
	}

	// Adjust the y axis to best fit the sweeping clip
	Vector3 vSpineBone( Vector3::ZeroType );
	Vector3 vPedPosition = VEC3V_TO_VECTOR3( pPed->GetTransform().GetPosition() );
	if( pPed->GetBonePosition( vSpineBone, BONETAG_SPINE3 ) )
	{
		vClosestPosition.z = vSpineBone.z;
		vPedPosition.z = vSpineBone.z;
	}

	TUNE_GROUP_BOOL(PED_SHOVING, bTestForIKPosition, false);
	TUNE_GROUP_BOOL(PED_SHOVING, bDoIKReaction, false);
	TUNE_GROUP_FLOAT(PED_SHOVING, fIKBlendInTime, 0.1f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(PED_SHOVING, fIKBlendOutTime, 0.25f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(PED_SHOVING, fImpusleScale, 1.0f, 0.0f, 100.0f, 0.01f);
	if(bTestForIKPosition)
	{
		Vector3 vHelperBonePosition(VEC3_ZERO);
		pPed->GetBonePosition(vHelperBonePosition, m_bUseLeftArm ? BONETAG_L_IK_HAND : BONETAG_R_IK_HAND);

		static dev_float CAPSULE_RADIUS = 0.05f;
		WorldProbe::CShapeTestCapsuleDesc pedCapsuleDesc;
		WorldProbe::CShapeTestHitPoint result;
		WorldProbe::CShapeTestResults probeResult(result);
		pedCapsuleDesc.SetResultsStructure( &probeResult );
		pedCapsuleDesc.SetCapsule( vPedPosition, vHelperBonePosition, CAPSULE_RADIUS );
		pedCapsuleDesc.SetIsDirected( true );
		pedCapsuleDesc.SetIncludeFlags( ArchetypeFlags::GTA_RAGDOLL_TYPE );
		pedCapsuleDesc.SetIncludeEntity( m_pTargetPed );
		pedCapsuleDesc.SetDomainForTest( WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS );
		pedCapsuleDesc.SetDoInitialSphereCheck( true );		

		const bool bHitRagdoll = WorldProbe::GetShapeTestManager()->SubmitTest( pedCapsuleDesc );
		if(bHitRagdoll)
		{
			if( (GetTimeInState() > ms_fMinTimeBeforeIkUse ) )
			{
				// Set the absolute arm ik target
				pPed->GetIkManager().SetAbsoluteArmIKTarget( m_bUseLeftArm ? crIKSolverArms::LEFT_ARM : crIKSolverArms::RIGHT_ARM, 
					probeResult[0].GetHitPosition(), 
					AIK_TARGET_WRT_IKHELPER,
					fIKBlendInTime,
					fIKBlendOutTime);
			
				if(!m_bDoTargetReaction && bDoIKReaction)
				{
					Vector3 vImpulseDir = vHelperBonePosition - vPedPosition;
					vImpulseDir.NormalizeFast();
					vImpulseDir *= fImpusleScale;

					CIkRequestBodyReact bodyReactRequest(RCC_VEC3V(probeResult[0].GetHitPosition()), 
						RC_VEC3V(vImpulseDir), 
						(int)probeResult[0].GetHitComponent());
					m_pTargetPed->GetIkManager().Request(bodyReactRequest);	

					m_bDoTargetReaction = true;
				}
			}
		}
	}

#if !__FINAL
	// Cache off debug information
	m_vClosestPoint = vClosestPosition;
#endif

	return FSM_Continue;
}

CTask::FSM_Return CTaskShovePed::StateShove_OnEnter( CPed* pPed )
{
	m_moveNetworkHelper.SendRequest( fwMvRequestId( "OnEnterOpenDoor",0xE98F58FE ) );

	if( !m_pLeftArmFilter )
	{
		static const atHashWithStringNotFinal sLeftArmFilterId( "UpperbodyFeathered_NoRightArm_filter",0x7320B763 );
		m_pLeftArmFilter = CGtaAnimManager::FindFrameFilter( sLeftArmFilterId.GetHash(), pPed );
		if( m_pLeftArmFilter )
			m_pLeftArmFilter->AddRef();
	}

	if( !m_pRightArmFilter )
	{
		static const atHashWithStringNotFinal sRightArmFilterId( "UpperbodyFeathered_NoLefttArm_filter",0xEFBF5E79 );
		m_pRightArmFilter = CGtaAnimManager::FindFrameFilter( sRightArmFilterId.GetHash(), pPed );
		if( m_pRightArmFilter )
		{
			m_pRightArmFilter->AddRef();
		}
	}

	// Force send MoVE params
	SendParams( pPed, true, true );

	return FSM_Continue;
}

CTask::FSM_Return CTaskShovePed::StateShove_OnUpdate( CPed* pPed )
{
	// Are we carrying a 2 handed weapon?
	const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponInfo() : NULL;
	if( !pWeaponInfo || pWeaponInfo->GetIsTwoHanded() )
	{
		return FSM_Quit;
	}

	// Is the designated ped facing the target?
	float fPedToTargetDot = CalculatePedDotToTargetPed(pPed, m_pTargetPed);
	if( fPedToTargetDot < ms_fPedDotToTargetThreshold )
	{
		return FSM_Quit;
	}

	// Are we near enough to target.
	Vector3 vClosestPosition = CalculateClosestTargetPedPosition(pPed, m_pTargetPed);
	Vector3 vPedPos = VEC3V_TO_VECTOR3( pPed->GetTransform().GetPosition() );
	vPedPos.z += ms_fPedTestHeightOffset;
	Vector3 vPedToClosestPos =  vClosestPosition - vPedPos;
	float fActorDistanceToTargetSq = vPedToClosestPos.XYMag2();

	// Are we outside the distance threshold?
	float fDistanceThreshold = Lerp( Abs( fPedToTargetDot ), ms_fMinActorToTargetDistance, ms_fMaxActorToTargetDistance );
	if( Abs( vPedToClosestPos.z ) > ms_fActorToTargetHeightThreshold || fActorDistanceToTargetSq > rage::square( fDistanceThreshold ) )
	{
		return FSM_Quit;
	}

	// Send over MoVE params
	if( !SendParams( pPed, false ) )
	{
		// If we changed arms then quit out of the current instance
		return FSM_Quit;
	}

	// Adjust the y axis to best fit the sweeping clip
	Vector3 vSpineBone( Vector3::ZeroType );
	Vector3 vPedPosition = VEC3V_TO_VECTOR3( pPed->GetTransform().GetPosition() );
	if( pPed->GetBonePosition( vSpineBone, BONETAG_SPINE3 ) )
	{
		vClosestPosition.z = vSpineBone.z;
		vPedPosition.z = vSpineBone.z;
	}

#if !__FINAL
	// Cache off debug information
	m_vClosestPoint = vClosestPosition;
#endif

	return FSM_Continue;
}

CTask::FSM_Return CTaskShovePed::StateBarge_OnEnter( CPed* pPed )
{
	m_moveNetworkHelper.SendRequest( fwMvRequestId( "OnEnterBarge",0xB6C73D62 ) );

	// Force send MoVE params
	SendParams( pPed, true, true );
	return FSM_Continue;
}

CTask::FSM_Return CTaskShovePed::StateBarge_OnUpdate( CPed* pPed )
{
	if( m_moveNetworkHelper.IsNetworkActive() )
	{
		// Terminate task once the anim has finished
		if( m_moveNetworkHelper.GetBoolean( ms_BargeAnimFinishedId ) )
		{
			return FSM_Quit;
		}
	}

	// Quit out if we are anything slower than our decided barge ratio
	bool bRunning = (pPed->GetMotionData()->GetIsRunning() || pPed->GetMotionData()->GetIsSprinting());
	if( !bRunning )
	{
		return FSM_Quit;
	}

	// Quit out if ped decided to change directions at the last moment
	float fPedDotToTarget = CalculatePedDotToTargetPed(pPed, m_pTargetPed);
	if( fPedDotToTarget < ms_fPedDotToTargetThresholdRunning )
	{
		return FSM_Quit;
	}

	Vector3 vClosestPosition = CalculateClosestTargetPedPosition(pPed, m_pTargetPed);

	// Calculate the interrupt phase which we will use to define the moment in which 
	// the animator would like to make contact on the door
	CalculateTargetAnimRate( pPed, vClosestPosition, ms_fRunningTargetOffset );

	// Send over MoVE params
	SendParams( pPed, false );

	// Adjust the y axis to best fit the sweeping clip
	Vector3 vSpineBone( Vector3::ZeroType );
	Vector3 vPedPosition = VEC3V_TO_VECTOR3( pPed->GetTransform().GetPosition() );
	if( pPed->GetBonePosition( vSpineBone, BONETAG_SPINE3 ) )
	{
		vClosestPosition.z = vSpineBone.z;
		vPedPosition.z = vSpineBone.z;
	}

#if !__FINAL
	// Cache off debug information
	m_vClosestPoint = vClosestPosition;
#endif

	return FSM_Continue;
}

bool CTaskShovePed::PrepareMoveNetwork( CPed* pPed )
{
	// Wait until the network worker is streamed in and initialized 
	if ( !m_moveNetworkHelper.IsNetworkActive() )
	{		
		m_moveNetworkHelper.RequestNetworkDef( CClipNetworkMoveInfo::ms_NetworkTaskOnFootOpenDoor );

		const CWeaponInfo* pEquippedWeaponInfo = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponInfo() : NULL;
		if( !pEquippedWeaponInfo )
		{
			SetState( State_Finish );
			return false;
		}

		// Determine whether or not we should use the barge or open door state
		bool bBarging = (pPed->GetMotionData()->GetIsRunning() || pPed->GetMotionData()->GetIsSprinting());
		bool bPushing = false;

		fwMvClipSetId weaponClipSet;
		if( pEquippedWeaponInfo->GetIsUnarmed() )
		{
			if(bBarging)
			{
				weaponClipSet = ms_UnarmedClipSetId;
			}
			else
			{
				bPushing = true;
				weaponClipSet = ms_UnarmedShoveClipSetId;
			}
		}
		else if( pEquippedWeaponInfo->GetIsTwoHanded() )
		{
			weaponClipSet = ms_2HandedClipSetId;
		}
		else
		{
			weaponClipSet = ms_1HandedClipSetId;
		}

		fwClipSet* pSet = fwClipSetManager::GetClipSet( weaponClipSet );
		taskAssert(pSet);

		if( pSet->Request_DEPRECATED() && m_moveNetworkHelper.IsNetworkDefStreamedIn( CClipNetworkMoveInfo::ms_NetworkTaskOnFootOpenDoor ) )
		{
			m_moveNetworkHelper.CreateNetworkPlayer( pPed, CClipNetworkMoveInfo::ms_NetworkTaskOnFootOpenDoor );
			
			TUNE_GROUP_FLOAT(PED_SHOVING, fBargingBlendTime, 0.125f, 0.0f, 1.0f, 0.01f);
			TUNE_GROUP_FLOAT(PED_SHOVING, fBlendTime, 0.25f, 0.0f, 1.0f, 0.01f);

			pPed->GetMovePed().SetSecondaryTaskNetwork( m_moveNetworkHelper.GetNetworkPlayer(), bBarging ? fBargingBlendTime : fBlendTime );

			m_moveNetworkHelper.SetClipSet( weaponClipSet );

			if( bBarging )
			{
				SetState( State_Barge );
			}
			else if(bPushing)
			{
				SetState( State_Push );
			}
			else
			{
				SetState( State_Shove );
			}

			// Success
			return true;
		}
	}

	// Waiting
	return false;
}

// Used to help determine the ped's relative direction to the door
float CTaskShovePed::CalculatePedDotToTargetPed(const CPed* pPed, CPed* pTargetPed)
{
	taskAssert( pPed );
	taskAssert( pTargetPed );

	Vector3 vDir = VEC3V_TO_VECTOR3(pTargetPed->GetTransform().GetPosition() - pPed->GetTransform().GetPosition());
	vDir.NormalizeFast();
	return DotProduct(VEC3V_TO_VECTOR3(pPed->GetTransform().GetB()), vDir);
}

// Calculate the door normal that will be used for the hand to door physics constraint
Vec3V_Out CTaskShovePed::CalculateConstraintNormal(const CPed* pPed, CPed* pTargetPed)
{
	taskAssert( pPed );
	taskAssert( pTargetPed );

	Vector3 vDir = VEC3V_TO_VECTOR3(pTargetPed->GetTransform().GetPosition() - pPed->GetTransform().GetPosition());
	return VECTOR3_TO_VEC3V(-vDir);
}


void CTaskShovePed::CalculateTargetAnimRate( CPed* pPed, const Vector3& vClosestPosition, float fTargetDistance )
{
	if( !m_bComputeTargetAnimRate )
	{
		const crClip* pClip = m_moveNetworkHelper.GetClip( ms_AnimClipId );
		if( pClip )
		{
			const fwMvClipSetId clipsetId = m_moveNetworkHelper.GetClipSetId();
			if( clipsetId != CLIP_SET_ID_INVALID )
			{
				fwClipSet* pClipSet = fwClipSetManager::GetClipSet( clipsetId );
				if( pClipSet )
				{
					CClipEventTags::FindEventPhase( pClip, CClipEventTags::Interruptible, m_fInterruptPhase );
				}
			}

			// We only care if we haven't already passed the target interrupt phase
			float fTimeRemaining = m_fInterruptPhase - m_moveNetworkHelper.GetFloat( ms_AnimClipPhaseId );
			if( fTimeRemaining > 0.0f )
			{
				// Determine how long it will take to get to the desired location with the current velocity
				float fVelocity = pPed->GetVelocity().Mag();
				if( fVelocity > 0.0f )
				{
					// Initialize the anim rate to as fast as possible
					m_fTargetAnimRate = ms_fMaxBargeAnimRate;

					float fDistToClosestPoint = vClosestPosition.Dist( VEC3V_TO_VECTOR3( pPed->GetTransform().GetPosition() ) );
					float fDistanceRemaining = MAX( fDistToClosestPoint - fTargetDistance, 0.0f );
					if( fDistanceRemaining > 0.0f )
					{
						float fTimeUntilArrival = fDistanceRemaining / fVelocity;
						m_fTargetAnimRate = MIN( fTimeRemaining / fTimeUntilArrival, ms_fMaxBargeAnimRate );
					}					
				}
			}

			// predict how long it will take to get there
			m_bComputeTargetAnimRate = true;
		}
	}
}

// Find the point on the door closest to the ped and figure out if we're not between the hinge and target end anymore
Vector3 CTaskShovePed::CalculateClosestTargetPedPosition(const CPed* UNUSED_PARAM(pPed), CPed* pTargetPed)
{
	//! Calculate position.
	Vector3 vTargetPos;
	pTargetPed->GetLockOnTargetAimAtPos(vTargetPos);
	return vTargetPos;
}

// Sends useful parameters to the associated MoVE network
bool CTaskShovePed::SendParams( CPed* pPed, bool bEnteringState, bool bForceDirection )
{	
	taskAssert( m_moveNetworkHelper.IsNetworkActive() );

	bool bOldUsingLeftArm = m_bUseLeftArm;

	float fPedToTargetDot = CalculatePedDotToTargetPed(pPed, m_pTargetPed);
	float fToTargetHeading = pPed->GetCurrentHeading();

	Vector3 vDir = VEC3V_TO_VECTOR3(m_pTargetPed->GetTransform().GetPosition() - pPed->GetTransform().GetPosition());
	vDir.z = 0.0f;
	if(vDir.Mag2() > SMALL_FLOAT)
	{
		vDir.NormalizeFast();
		fToTargetHeading = fwAngle::LimitRadianAngle(rage::Atan2f(vDir.x, -vDir.y));
	}

	bool bLeftArm = SubtractAngleShorter(GetPed()->GetCurrentHeading(), fToTargetHeading) > 0.0f;

	//! Quite complex to fix this in task, so disallow.
	TUNE_GROUP_BOOL(PED_SHOVING, bEnableInTaskArmSwitching, false);
	if( !bEnteringState && ((bOldUsingLeftArm != bLeftArm) && !bEnableInTaskArmSwitching) )
	{
		pPed->SetLastPedShoved(NULL);
		return false;
	}

	m_bUseLeftArm = bLeftArm;

	// Force the actor direction to match the target direction
	if( bForceDirection )
	{
		m_fActorDirection = fPedToTargetDot;
	}
	else
	{
		Approach( m_fActorDirection, fPedToTargetDot, ms_fDirectionBlendRate, fwTimer::GetTimeStep() );
	}

	m_moveNetworkHelper.SetFlag( m_bUseLeftArm, ms_ActorUseLeftArmId );
	m_moveNetworkHelper.SetFloat( ms_ActorDirectionId, m_fActorDirection );

	m_moveNetworkHelper.SetFilter( m_pLeftArmFilter, ms_LeftArmFilterId );
	m_moveNetworkHelper.SetFilter( m_pRightArmFilter, ms_RightArmFilterId );

	// If we found an anim interrupt event calculate the anim rate to achieve that 
	if( m_bComputeTargetAnimRate )
	{
		float fTimeRemaining = m_fInterruptPhase - m_moveNetworkHelper.GetFloat( ms_AnimClipPhaseId );
		if( fTimeRemaining > 0 )
		{
			m_moveNetworkHelper.SetFloat( ms_AnimClipRateId, m_fTargetAnimRate );
		}
		else
		{
			m_moveNetworkHelper.SetFloat( ms_AnimClipRateId, 1.0f );
		}
	}
	// Lets try this as a way to reset the anim clip rate and avoid bug #596924
	else
	{
		m_moveNetworkHelper.SetFloat( ms_AnimClipRateId, 1.0f );
	}

	// Everything is fine
	return true;
}

#if !__FINAL

// PURPOSE: Display debug information specific to this task
void CTaskShovePed::Debug() const
{
#if __BANK
	const Matrix34& rMat = MAT34V_TO_MATRIX34( m_pTargetPed->GetMatrix());
	rage::grcDebugDraw::Sphere(rMat.d, 0.05f, Color_blue );	

	grcDebugDraw::Sphere(m_vClosestPoint, 0.05f, Color_green, true, 1);
	grcDebugDraw::Line(rMat.d, rMat.d + rMat.a, Color_red, 1 );
	grcDebugDraw::Line(rMat.d, rMat.d + rMat.b, Color_green, 1 );
	grcDebugDraw::Line(rMat.d, rMat.d + rMat.c, Color_blue, 1 );

	// Draw the distance squared to the target
	char tempString[128];
	grcDebugDraw::Text( rMat.d + Vector3(0.0f, 0.0f, 0.5f), Color_white, tempString, true, 1 );
#endif

	// Base class
	CTask::Debug();
}

// return the name of the given task as a string

const char * CTaskShovePed::GetStaticStateName( s32 iState  )
{	
	// Make sure FSM state is valid
	taskAssert( iState < State_Finish );
	switch( iState )
	{
	case State_Initial: return "Initial";
	case State_Shove: return "Shove";
	case State_Push: return "Push";
	case State_Barge: return "Barge";
	case State_Finish: return "Finish";
	default: { Assert( false ); return "Unknown"; }
	}
}
#endif //!__FINAL
