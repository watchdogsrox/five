// FILE :    TaskShoved.h

// File header
#include "Task/Response/TaskShoved.h"

// Rage header
#include "fwanimation/clipsets.h"

// Game headers
#include "animation/AnimDefines.h"
#include "animation/EventTags.h"
#include "Peds/ped.h"
#include "Peds/PedIntelligence.h"
#include "system/control.h"
#include "task/Response/TaskAgitated.h"

AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////
// CTaskShoved
////////////////////////////////////////////////////////////////////////////////

CTaskShoved::Tunables CTaskShoved::sm_Tunables;
IMPLEMENT_RESPONSE_TASK_TUNABLES(CTaskShoved, 0x72de3e5f);

CTaskShoved::CTaskShoved(const CPed* pShover)
: m_pShover(pShover)
{
	SetInternalTaskType(CTaskTypes::TASK_SHOVED);
}

CTaskShoved::~CTaskShoved()
{

}

bool CTaskShoved::IsValid()
{
	//Ensure the clip set is streamed in.
	if(!fwClipSetManager::IsStreamedIn_DEPRECATED(CLIP_SET_REACTION_SHOVE))
	{
		return false;
	}

	return true;
}

#if !__FINAL
void CTaskShoved::Debug() const
{
#if DEBUG_DRAW
#endif

	CTask::Debug();
}
#endif // !__FINAL

#if !__FINAL
const char* CTaskShoved::GetStaticStateName(s32 iState)
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

bool CTaskShoved::CanInterrupt() const
{
	//Ensure the event is valid.
	if(!GetClipHelper()->IsEvent(CClipEventTags::Interruptible))
	{
		return false;
	}

	return true;
}

fwMvClipId CTaskShoved::ChooseClip() const
{
	//Check the direction to the shover.
	CDirectionHelper::Direction nDirection = CDirectionHelper::CalculateClosestDirection(GetPed()->GetTransform().GetPosition(),
		GetPed()->GetCurrentHeading(), m_pShover->GetTransform().GetPosition());
	switch(nDirection)
	{
		case CDirectionHelper::D_Left:
		{
			static fwMvClipId s_ClipId("SHOVED_LEFT",0x8649FF12);
			return s_ClipId;
		}
		case CDirectionHelper::D_Right:
		{
			static fwMvClipId s_ClipId("SHOVED_RIGHT",0x870622D2);
			return s_ClipId;
		}
		case CDirectionHelper::D_FrontLeft:
		case CDirectionHelper::D_FrontRight:
		{
			static fwMvClipId s_ClipId("SHOVED_FRONT",0xA7C0A113);
			return s_ClipId;
		}
		case CDirectionHelper::D_BackLeft:
		case CDirectionHelper::D_BackRight:
		{
			static fwMvClipId s_ClipId("SHOVED_BACK",0xF0C1071D);
			return s_ClipId;
		}
		default:
		{
			return CLIP_ID_INVALID;
		}
	}
}

bool CTaskShoved::HasLocalPlayerMadeInput() const
{
	//Check if the ped is the local player.
	const CPed* pPed = GetPed();
	if(pPed->IsLocalPlayer())
	{
		//Check if the player is trying to move.
		float fThreshold = 0.1f;
		if(Abs(pPed->GetControlFromPlayer()->GetPedWalkLeftRight().GetNorm() - 0.5f) > fThreshold)
		{
			return true;
		}
		else if(Abs(pPed->GetControlFromPlayer()->GetPedWalkUpDown().GetNorm() - 0.5f) > fThreshold)
		{
			return true;
		}

		//Check if the player is trying to aim.
		if(pPed->GetPlayerInfo()->IsAiming())
		{
			return true;
		}

		//Check if the player is trying to fire.
		if(pPed->GetPlayerInfo()->IsFiring())
		{
			return true;
		}
	}

	return false;
}

bool CTaskShoved::ShouldInterrupt() const
{
	//Check if we can interrupt.
	if(CanInterrupt())
	{
		//Check if the local player has made input.
		if(HasLocalPlayerMadeInput())
		{
			return true;
		}
	}

	return false;
}

void CTaskShoved::CleanUp()
{

}

aiTask* CTaskShoved::Copy() const
{
	//Create the task.
	CTaskShoved* pTask = rage_new CTaskShoved(m_pShover);

	return pTask;
}

CTask::FSM_Return CTaskShoved::ProcessPreFSM()
{
	//Ensure the shover is valid.
	if(!m_pShover)
	{
		return FSM_Quit;
	}

	//Set the desired heading.
	GetPed()->SetDesiredHeading(VEC3V_TO_VECTOR3(m_pShover->GetTransform().GetPosition()));

	return FSM_Continue;
}

CTask::FSM_Return CTaskShoved::UpdateFSM( const s32 iState, const FSM_Event iEvent )
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
			FSM_OnExit
				React_OnExit();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskShoved::Start_OnUpdate()
{
	//Assert that the task is valid.
	taskAssert(IsValid());

	//React to the shove.
	SetState(State_React);

	return FSM_Continue;
}

void CTaskShoved::React_OnEnter()
{
	//Choose the clip.
	fwMvClipId clipId = ChooseClip();
	taskAssert(clipId != CLIP_ID_INVALID);

	//Play the clip.
	StartClipBySetAndClip(GetPed(), CLIP_SET_REACTION_SHOVE, clipId, NORMAL_BLEND_IN_DELTA, CClipHelper::TerminationType_OnFinish);

	//Force an animation update this frame.
	GetPed()->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true);
}

CTask::FSM_Return CTaskShoved::React_OnUpdate()
{
	//Check if the clip has finished.
	if(!GetClipHelper() || GetClipHelper()->IsHeldAtEnd())
	{
		//Finish the task.
		SetState(State_Finish);
	}
	//Check if we should interrupt.
	else if(ShouldInterrupt())
	{
		//Finish the task.
		SetState(State_Finish);
	}

	return FSM_Continue;
}

void CTaskShoved::React_OnExit()
{
	//Stop the clip.
	StopClip(NORMAL_BLEND_OUT_DELTA);

	//Check if the shover is valid.
	if(m_pShover)
	{
		//Check if the shover is agitated.
		CTaskAgitated* pTask = CTaskAgitated::FindAgitatedTaskForPed(*m_pShover);
		if(pTask && (GetPed() == pTask->GetAgitator()))
		{
			//Reset the closest distance.
			pTask->ResetClosestDistance();
		}
	}

	//Check if we are the local player.
	if(GetPed()->IsLocalPlayer())
	{
		//Increase battle awareness.
		GetPed()->GetPedIntelligence()->IncreaseBattleAwareness(CPedIntelligence::BATTLE_AWARENESS_SHOVED, false, false);
	}
}
