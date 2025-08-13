// FILE :    TaskReactToBuddyShot.h

// File header
#include "Task/Combat/Subtasks/TaskReactToBuddyShot.h"

// Game headers
#include "Peds/ped.h"
#include "Peds/PedIntelligence.h"
#include "task/Motion/Locomotion/TaskMotionAiming.h"
#include "task/Response/TaskReactInDirection.h"
#include "task/Weapons/Gun/TaskGun.h"

AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////
// CTaskReactToBuddyShot
////////////////////////////////////////////////////////////////////////////////

u32 CTaskReactToBuddyShot::s_uLastTimeUsedOnScreen = 0;

CTaskReactToBuddyShot::Tunables CTaskReactToBuddyShot::sm_Tunables;
IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskReactToBuddyShot, 0xe0d2777b);

CTaskReactToBuddyShot::CTaskReactToBuddyShot(const CAITarget& rReactTarget, const CAITarget& rAimTarget)
: m_ReactTarget(rReactTarget)
, m_AimTarget(rAimTarget)
, m_uRunningFlags(0)
{
	SetInternalTaskType(CTaskTypes::TASK_REACT_TO_BUDDY_SHOT);
}

CTaskReactToBuddyShot::~CTaskReactToBuddyShot()
{

}

fwMvClipSetId CTaskReactToBuddyShot::ChooseClipSet(const CPed& rPed)
{
	//Check if the weapon manager is valid.
	const CPedWeaponManager* pWeaponManager = rPed.GetWeaponManager();
	if(pWeaponManager)
	{
		//Check if the equipped weapon is valid.
		const CWeapon* pWeapon = pWeaponManager->GetEquippedWeapon();
		if(pWeapon)
		{
			//Check if the weapon is a pistol.
			if(pWeapon->GetWeaponInfo()->GetGroup() == WEAPONGROUP_PISTOL)
			{
				return CLIP_SET_COMBAT_BUDDY_SHOT_PISTOL;
			}
			//Check if the weapon is a rifle.
			else if(pWeapon->GetWeaponInfo()->GetGroup() == WEAPONGROUP_SMG || pWeapon->GetWeaponInfo()->GetGroup() == WEAPONGROUP_RIFLE)
			{
				return CLIP_SET_COMBAT_BUDDY_SHOT_RIFLE;
			}
		}
	}

	return CLIP_SET_ID_INVALID;
}

bool CTaskReactToBuddyShot::HasBeenReactedTo(const CPed& rPed, const CPed& rTarget)
{
	//Check the direction.
	CDirectionHelper::Direction nDirection = CDirectionHelper::CalculateClosestDirection(
		rPed.GetTransform().GetPosition(), rPed.GetCurrentHeading(), rTarget.GetTransform().GetPosition());
	switch(nDirection)
	{
		case CDirectionHelper::D_FrontLeft:
		case CDirectionHelper::D_FrontRight:
		{
			return rTarget.GetPedConfigFlag(CPED_CONFIG_FLAG_HasShotBeenReactedToFromFront);
		}
		case CDirectionHelper::D_BackLeft:
		case CDirectionHelper::D_BackRight:
		{
			return rTarget.GetPedConfigFlag(CPED_CONFIG_FLAG_HasShotBeenReactedToFromBack);
		}
		case CDirectionHelper::D_Left:
		{
			return rTarget.GetPedConfigFlag(CPED_CONFIG_FLAG_HasShotBeenReactedToFromLeft);
		}
		case CDirectionHelper::D_Right:
		{
			return rTarget.GetPedConfigFlag(CPED_CONFIG_FLAG_HasShotBeenReactedToFromRight);
		}
		default:
		{
			return false;
		}
	}
}

bool CTaskReactToBuddyShot::IsValid(const CPed& rPed, const CAITarget& rReactTarget, const CAITarget& rAimTarget)
{
	//Ensure the time is valid.
	static dev_u32 s_uMinTime = 250;
	if(CTimeHelpers::GetTimeSince(s_uLastTimeUsedOnScreen) < s_uMinTime)
	{
		aiDebugf1("Ped %s : TimeSince s_uLastTimeUsedOnScreen %i (s_uMinTime - %i)", rPed.GetDebugName(), CTimeHelpers::GetTimeSince(s_uLastTimeUsedOnScreen), s_uMinTime);
		return false;
	}

	//Ensure the clip set is valid.
	fwMvClipSetId clipSetId = ChooseClipSet(rPed);
	if(clipSetId == CLIP_SET_ID_INVALID)
	{
		aiDebugf1("Ped %s : clipSetId = CLIP_SET_ID_INVALID", rPed.GetDebugName());
		return false;
	}

	//Ensure the react is valid.
	if(!CTaskReactInDirection::IsValid(clipSetId, rReactTarget))
	{
		aiDebugf1("Ped %s : clipSetId %s : streamed %s : m_ReactTarget %d, %p", rPed.GetDebugName(), clipSetId.GetCStr(), fwClipSetManager::IsStreamedIn_DEPRECATED(clipSetId) ? "true" : "false", rReactTarget.GetIsValid(), rReactTarget.GetEntity());
		return false;
	}

	//Ensure the aim target is valid.
	if(!rAimTarget.GetIsValid())
	{
		aiDebugf1("Ped %s : rAimTarget %d, %p", rPed.GetDebugName(), rAimTarget.GetIsValid(), rAimTarget.GetEntity());
		return false;
	}

	//Ensure the target position is valid.
	Vec3V vTargetPosition;
	if(!rAimTarget.GetPosition(RC_VECTOR3(vTargetPosition)))
	{
		return false;
	}

	//Ensure the ped is facing their target.
	Vec3V vForward = rPed.GetTransform().GetForward();
	Vec3V vPedToTarget = Subtract(vTargetPosition, rPed.GetTransform().GetPosition());
	Vec3V vPedToTargetDirection = NormalizeFastSafe(vPedToTarget, Vec3V(V_ZERO));
	ScalarV scDot = Dot(vForward, vPedToTargetDirection);
	static dev_float s_fMinDot = 0.95f;
	ScalarV scMinDot = ScalarVFromF32(s_fMinDot);
	if(IsLessThanAll(scDot, scMinDot))
	{
		aiDebugf1("Ped %s : scDot %.2f : scMinDot %.2f", rPed.GetDebugName(), scDot.Getf(), scMinDot.Getf());
		return false;
	}

	return true;
}

#if !__FINAL
void CTaskReactToBuddyShot::Debug() const
{
#if DEBUG_DRAW
#endif

	CTask::Debug();
}
#endif // !__FINAL

#if !__FINAL
const char* CTaskReactToBuddyShot::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_React",
		"State_Finish",
	};

	return aStateNames[iState];
}
#endif

const CPed* CTaskReactToBuddyShot::GetTargetPed() const
{
	//Ensure the entity is valid.
	const CEntity* pEntity = m_ReactTarget.GetEntity();
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

bool CTaskReactToBuddyShot::MustTargetBeValid() const
{
	//Check the state.
	switch(GetState())
	{
		case State_React:
		{
			return false;
		}
		default:
		{
			break;
		}
	}

	return true;
}

void CTaskReactToBuddyShot::CleanUp()
{
	//Blend with the aim pose.
	GetPed()->SetPedResetFlag(CPED_RESET_FLAG_InstantBlendToAim, true);
}

aiTask* CTaskReactToBuddyShot::Copy() const
{
	//Create the task.
	CTaskReactToBuddyShot* pTask = rage_new CTaskReactToBuddyShot(m_ReactTarget, m_AimTarget);

	return pTask;
}

CTask::FSM_Return CTaskReactToBuddyShot::ProcessPreFSM()
{
	//Check if the target must be valid.
	if(MustTargetBeValid())
	{
		//Ensure the target is valid.
		if(!m_ReactTarget.GetIsValid())
		{
			return FSM_Quit;
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskReactToBuddyShot::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin
	
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_React)
			FSM_OnEnter
				React_OnEnter();
			FSM_OnUpdate
				return React_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskReactToBuddyShot::Start_OnUpdate()
{
	//Check if the ped is on-screen.
	if(GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_VisibleOnScreen))
	{
		//Set the last time used.
		s_uLastTimeUsedOnScreen = fwTimer::GetTimeInMilliseconds();
	}

	//React.
	if(GetPed()->IsNetworkClone())
	{
		//If we are a clone - check the clip set has streamed. Give it a small chance to succeed, otherwise bail.
		fwMvClipSetId clipSetId = ChooseClipSet(*GetPed());
		if(!CTaskReactInDirection::IsValid(clipSetId, m_ReactTarget))
		{
			if(GetTimeInState() > 0.25f)
			{
				return FSM_Quit;
			}
			return FSM_Continue;
		}
	}
	SetState(State_React);

	return FSM_Continue;
}

void CTaskReactToBuddyShot::React_OnEnter()
{
	//Choose the clip set.
	fwMvClipSetId clipSetId = ChooseClipSet(*GetPed());
	taskAssert(clipSetId != CLIP_SET_ID_INVALID);

	//Create the task.
	taskAssertf(CTaskReactInDirection::IsValid(clipSetId, m_ReactTarget), "Ped %s : clipSetId %s : streamed %s : m_ReactTarget %d, %p", GetPed() ? GetPed()->GetDebugName() : "No Ped", clipSetId.GetCStr(), fwClipSetManager::IsStreamedIn_DEPRECATED(clipSetId) ? "true" : "false", m_ReactTarget.GetIsValid(), m_ReactTarget.GetEntity());
	CTaskReactInDirection* pTask = rage_new CTaskReactInDirection(clipSetId, m_ReactTarget,
		CTaskReactInDirection::CF_Use5Directions | CTaskReactInDirection::CF_Interrupt);
	pTask->SetBlendOutDuration(REALLY_SLOW_BLEND_DURATION);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskReactToBuddyShot::React_OnUpdate()
{
	//Force the motion state.
	GetPed()->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Aiming);
	GetPed()->SetPedResetFlag(CPED_RESET_FLAG_DontQuitMotionAiming, true);

	//Set the move/blend ratio.
	GetPed()->GetMotionData()->SetCurrentMoveBlendRatio(MOVEBLENDRATIO_STILL);

	//Check if we have not processed the direction.
	if(!m_uRunningFlags.IsFlagSet(RF_HasProcessedDirection))
	{
		//Check if the sub-task is valid.
		taskAssert(!GetSubTask() || (GetSubTask()->GetTaskType() == CTaskTypes::TASK_REACT_IN_DIRECTION));
		const CTaskReactInDirection* pSubTask = static_cast<CTaskReactInDirection *>(GetSubTask());
		if(pSubTask)
		{
			//Check if the direction is valid.
			CDirectionHelper::Direction nDirection = pSubTask->GetDirection();
			if(nDirection != CDirectionHelper::D_Invalid)
			{
				//Set the flag.
				m_uRunningFlags.SetFlag(RF_HasProcessedDirection);

				//Check if the target ped is valid.
				CPed* pTargetPed = const_cast<CPed *>(GetTargetPed());
				if(pTargetPed)
				{
					//Check the direction.
					switch(nDirection)
					{
						case CDirectionHelper::D_FrontLeft:
						case CDirectionHelper::D_FrontRight:
						{
							pTargetPed->SetPedConfigFlag(CPED_CONFIG_FLAG_HasShotBeenReactedToFromFront, true);
							break;
						}
						case CDirectionHelper::D_BackLeft:
						case CDirectionHelper::D_BackRight:
						{
							pTargetPed->SetPedConfigFlag(CPED_CONFIG_FLAG_HasShotBeenReactedToFromBack, true);
							break;
						}
						case CDirectionHelper::D_Left:
						{
							pTargetPed->SetPedConfigFlag(CPED_CONFIG_FLAG_HasShotBeenReactedToFromLeft, true);
							break;
						}
						case CDirectionHelper::D_Right:
						{
							pTargetPed->SetPedConfigFlag(CPED_CONFIG_FLAG_HasShotBeenReactedToFromRight, true);
							break;
						}
						default:
						{
							break;
						}
					}
				}
			}
		}
	}

	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}

	return FSM_Continue;
}
