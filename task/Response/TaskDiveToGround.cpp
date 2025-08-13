// FILE :    TaskDiveToGround.cpp
// CREATED : 07/02/2012

//rage headers
#include "math/angmath.h"

// game headers
#include "Peds/ped.h"
#include "Task/Movement/TaskGetUp.h"
#include "Task/Response/TaskDiveToGround.h"
#include "Task/Physics/TaskNM.h"
#include "Task/Physics/TaskNMJumpRollFromRoadVehicle.h"
#include "event/Events.h"

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CtaskDiveToGround
//////////////////////////////////////////////////////////////////////////

CTaskDiveToGround::CTaskDiveToGround(Vec3V_ConstRef vDiveDirection, const fwMvClipSetId& clipSet)
: m_vDiveDirection(vDiveDirection)
, m_clipSet(clipSet)
, m_fBlendInDelta(FAST_BLEND_IN_DELTA)
{
	SetInternalTaskType(CTaskTypes::TASK_DIVE_TO_GROUND);

	//Assert that the dive direction is valid.
	taskAssert(!IsCloseAll(m_vDiveDirection, Vec3V(V_ZERO), ScalarVFromF32(SMALL_FLOAT)));
}

CTaskDiveToGround::~CTaskDiveToGround()
{
}

aiTask* CTaskDiveToGround::Copy() const
{
	CTaskDiveToGround* pTask = rage_new CTaskDiveToGround(m_vDiveDirection, m_clipSet);
	pTask->SetBlendInDelta(m_fBlendInDelta);

	return pTask;
}

bool CTaskDiveToGround::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	//Check if the priority is not immediate.
	if(iPriority != ABORT_PRIORITY_IMMEDIATE)
	{
		//Check if the event is valid.
		if(pEvent)
		{
			//Check the event type.
			switch(((CEvent *)pEvent)->GetEventType())
			{
				case EVENT_EXPLOSION:
				case EVENT_SCRIPT_COMMAND: //Don't allow scripters to interrupt.  The blend looks like crap.
				{
					return false;
				}
				default:
				{
					break;
				}
			}
		}
	}

	return CTask::ShouldAbort(iPriority, pEvent);
}

#if !__FINAL
void CTaskDiveToGround::Debug() const
{
#if DEBUG_DRAW && __DEV
	grcDebugDraw::Axis(m_lastMatrix, 1.0f, true);
#endif
}
#endif // !__FINAL

#if !__FINAL
const char* CTaskDiveToGround::GetStaticStateName( s32 iState )
{
	Assert(iState>=State_Start&&iState<=State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Dive",
		"State_GetUp",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

CTask::FSM_Return CTaskDiveToGround::ProcessPreFSM()
{
	//Ensure the ped is valid.
	CPed* pPed = GetPed();
	if(!pPed)
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskDiveToGround::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_Dive)
			FSM_OnEnter
				Dive_OnEnter();
			FSM_OnUpdate
				return Dive_OnUpdate();
			FSM_OnExit
				Dive_OnExit();

		FSM_State(State_GetUp)
			FSM_OnEnter
				GetUp_OnEnter();
			FSM_OnUpdate
				return GetUp_OnUpdate();
			FSM_OnExit
				GetUp_OnExit();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskDiveToGround::Start_OnUpdate()
{
	//Choose the state.
	SetState(State_Dive);

	return FSM_Continue;
}

void CTaskDiveToGround::Dive_OnEnter()
{
	fwMvClipId clipHash;

	PickDiveReactionClip(clipHash);

	if (m_clipSet.GetHash()!=0  && clipHash.GetHash()!=0)
	{
		StartClipBySetAndClip(GetPed(), m_clipSet , clipHash, 0, 0, 0, m_fBlendInDelta, CClipHelper::TerminationType_OnFinish);

		// TODO - delete this once we have fixed up explosion assets
		// search for critical frame event to tell us a good start phase
		if (GetClipHelper() && GetClipHelper()->GetClip())
		{
			float startPhase = 0.0f;
			CClipEventTags::FindEventPhase(GetClipHelper()->GetClip(), CClipEventTags::CriticalFrame, startPhase);
			GetClipHelper()->SetPhase(startPhase);

			static float s_fMaxRateVariation = 0.2f;
			GetClipHelper()->SetRate(1.0f + fwRandom::GetRandomNumberInRange(-s_fMaxRateVariation, s_fMaxRateVariation));
		}
	}

	CTask* pTaskNM = rage_new CTaskNMJumpRollFromRoadVehicle(1000, 10000, false);
	CEventSwitch2NM event(10000, pTaskNM);
	GetPed()->SetActivateRagdollOnCollisionEvent(event);
}

CTask::FSM_Return CTaskDiveToGround::Dive_OnUpdate()
{
	CPed* pPed = GetPed();

	if ( !GetClipHelper() || IsClipConsideredFinished() )
	{
		SetState(State_GetUp);
		return FSM_Continue;
	}
	else
	{
		// Flag to activate on collision based on whether we have a CanSwitchToNm event
		pPed->SetActivateRagdollOnCollision(GetClipHelper()->IsEvent(CClipEventTags::CanSwitchToNm));
	}

	return FSM_Continue;
}

void CTaskDiveToGround::Dive_OnExit()
{
	CPed* pPed = GetPed();

	pPed->SetActivateRagdollOnCollision(false);
	pPed->ClearActivateRagdollOnCollisionEvent();
}

void CTaskDiveToGround::GetUp_OnEnter()
{
	//@@: location CTASKDIVETOGROUND_GETUPONENTER
	CTask* pTaskGetUp = rage_new CTaskGetUp();
	SetNewTask(pTaskGetUp);
}

CTask::FSM_Return CTaskDiveToGround::GetUp_OnUpdate()
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_GET_UP))
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}

void CTaskDiveToGround::GetUp_OnExit()
{
}


void CTaskDiveToGround::PickDiveReactionClip( fwMvClipId& clipHash)
{
	CPed* pPed = GetPed();

	static const fwMvClipId forwardClip("react_blown_forwards",0x11D9ECFC);
	static const fwMvClipId backwardClip("react_blown_backwards",0x5820E794);
	static const fwMvClipId rightClip("react_blown_left",0x62FFC066);
	static const fwMvClipId leftClip("react_blown_right",0x4D39AB07);

	// pick the correct clip to play back based on the angle of the dive
	if (pPed)
	{
		Mat34V matrix;
		LookAt(matrix, pPed->GetMatrix().d(), pPed->GetMatrix().d() + m_vDiveDirection, Vec3V(V_Z_AXIS_WZERO));

		Vec3V eulers = Mat34VToEulersXYZ(matrix);

		float angle = eulers.GetZf();

		const float fCurrentHeading = fwAngle::LimitRadianAngleSafe(pPed->GetCurrentHeading());
		const float fDesiredHeading = fwAngle::LimitRadianAngleSafe(angle);

		const float	fHeadingDelta = SubtractAngleShorter(fDesiredHeading, fCurrentHeading);
#if __DEV
		Matrix34 mat(M34_IDENTITY);
		mat.RotateZ(fHeadingDelta);
		mat.Dot(MAT34V_TO_MATRIX34(pPed->GetMatrix()));
		m_lastMatrix = mat;
#endif //__DEV

		static const float rearAngle = (3.0f*PI / 4.0f);
		static const float frontAngle = (PI / 4.0f);

		if (abs(fHeadingDelta) > rearAngle)
		{
			clipHash = forwardClip;
		}
		else if (abs(fHeadingDelta) < frontAngle)
		{
			clipHash = backwardClip;
		}
		else if (fHeadingDelta>0.0f)
		{
			clipHash = leftClip;
		}
		else
		{
			clipHash = rightClip;
		}
	}
}

CTaskInfo* CTaskDiveToGround::CreateQueriableState() const
{
	return rage_new CClonedDiveToGroundInfo(VEC3V_TO_VECTOR3(m_vDiveDirection), m_clipSet);
}

CTask::FSM_Return CTaskDiveToGround::UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent)
{
	return UpdateFSM(iState, iEvent);
}
