// FILE :    TaskShove.h

// File header
#include "Task/Response/TaskShove.h"

// Rage headers
#include "fwanimation/clipsets.h"
#include "grcore/debugdraw.h"

// Game headers
#include "animation/AnimDefines.h"
#include "animation/EventTags.h"
#include "event/Events.h"
#include "event/EventShocking.h"
#include "event/ShockingEvents.h"
#include "Peds/ped.h"
#include "peds/PedIntelligence.h"
#include "physics/gtaArchetype.h"
#include "physics/WorldProbe/shapetestspheredesc.h"
#include "task/Response/TaskShoved.h"

AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////
// CTaskShove
////////////////////////////////////////////////////////////////////////////////

CTaskShove::Tunables CTaskShove::sm_Tunables;
IMPLEMENT_RESPONSE_TASK_TUNABLES(CTaskShove, 0x1e9702b6);

CTaskShove::CTaskShove(CPed* pTarget, fwFlags8 uConfigFlags)
: m_pTarget(pTarget)
, m_uConfigFlags(uConfigFlags)
, m_uRunningFlags(0)
{
	SetInternalTaskType(CTaskTypes::TASK_SHOVE);
}

CTaskShove::~CTaskShove()
{

}

bool CTaskShove::IsValid(const CPed& rPed, const CPed& rTarget)
{
	//Ensure the ped is not playing a gesture.
	if(rPed.IsPlayingAGestureAnim())
	{
		return false;
	}

	//Ensure the target is not in motion.
	if(rTarget.IsInMotion())
	{
		return false;
	}

	//Ensure the clip set is streamed in.
	if(!fwClipSetManager::IsStreamedIn_DEPRECATED(CLIP_SET_REACTION_SHOVE))
	{
		return false;
	}

	//Ensure the distance is valid.
	ScalarV scDistSq = DistSquared(rPed.GetTransform().GetPosition(), rTarget.GetTransform().GetPosition());
	ScalarV scMaxDistSq = ScalarVFromF32(square(sm_Tunables.m_MaxDistance));
	if(IsGreaterThanAll(scDistSq, scMaxDistSq))
	{
		return false;
	}

	//Ensure the dot is valid.
	Vec3V vPedForward = rPed.GetTransform().GetForward();
	Vec3V vPedToTarget = Subtract(rTarget.GetTransform().GetPosition(), rPed.GetTransform().GetPosition());
	Vec3V vPedToTargetDirection = NormalizeFastSafe(vPedToTarget, Vec3V(V_ZERO));
	ScalarV scDot = Dot(vPedForward, vPedToTargetDirection);
	ScalarV scMinDot = ScalarVFromF32(sm_Tunables.m_MinDot);
	if(IsLessThanAll(scDot, scMinDot))
	{
		return false;
	}

	return true;
}

#if !__FINAL
void CTaskShove::Debug() const
{
#if DEBUG_DRAW
#endif

	CTask::Debug();
}
#endif // !__FINAL

#if !__FINAL
const char* CTaskShove::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Shove",
		"State_Finish",
	};

	return aStateNames[iState];
}
#endif

bool CTaskShove::CanInterrupt() const
{
	//Ensure the event is valid.
	if(!GetClipHelper()->IsEvent(CClipEventTags::Interruptible))
	{
		return false;
	}

	return true;
}

fwMvClipId CTaskShove::ChooseClip() const
{
	//Choose a random clip.
	float fRandom = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
	if(fRandom <= 0.33f)
	{
		static fwMvClipId s_ClipId("SHOVE_VAR_A",0x1F286858);
		return s_ClipId;
	}
	else if(fRandom <= 0.66f)
	{
		static fwMvClipId s_ClipId("SHOVE_VAR_B",0x417C2CFF);
		return s_ClipId;
	}
	else
	{
		static fwMvClipId s_ClipId("SHOVE_VAR_C",0x8F34486A);
		return s_ClipId;
	}
}

bool CTaskShove::HasMadeContact(RagdollComponent nRagdollComponent) const
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Calculate the position.
	phBoundComposite* pBound = GetPed()->GetRagdollInst()->GetCacheEntry()->GetBound();
	Matrix34 mBound;
	mBound.Dot(RCC_MATRIX34(pBound->GetCurrentMatrix(nRagdollComponent)), RCC_MATRIX34(GetPed()->GetRagdollInst()->GetMatrix()));
	Vector3 vPosition = mBound.d;

	//Grab the radius.
	float fRadius = sm_Tunables.m_RadiusForContact;

#if DEBUG_DRAW
	//Check if we should render the contact.
	if(sm_Tunables.m_Rendering.m_Enabled && sm_Tunables.m_Rendering.m_Contact)
	{
		//Render the contact.
		grcDebugDraw::Sphere(vPosition, fRadius, Color_blue);
	}
#endif

	//Submit a probe.
	WorldProbe::CShapeTestSphereDesc desc;
	WorldProbe::CShapeTestFixedResults<> results;
	desc.SetSphere(vPosition, fRadius);
	desc.SetResultsStructure(&results);
	desc.SetExcludeEntity(pPed);
	desc.SetIncludeFlags(ArchetypeFlags::GTA_WEAPON_TYPES);
	desc.SetTypeFlags(ArchetypeFlags::GTA_PED_TYPE);
	if(!WorldProbe::GetShapeTestManager()->SubmitTest(desc))
	{
		return false;
	}

	//Ensure the result is the target.
	if(m_pTarget.Get() != results[0].GetHitEntity())
	{
		return false;
	}

	return true;
}

void CTaskShove::ProcessContacts()
{
	//Check if either of the hands have made contact.
	if(HasMadeContact(RAGDOLL_HAND_LEFT) || HasMadeContact(RAGDOLL_HAND_RIGHT))
	{
		//Set the flag.
		m_uRunningFlags.SetFlag(RF_HasMadeContact);

		//Notify the target that they have been shoved.
		CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY, rage_new CTaskShoved(GetPed()));
		m_pTarget->GetPedIntelligence()->AddEvent(event);
	}
}

bool CTaskShove::ShouldInterrupt() const
{
	//Check if we can interrupt.
	if(CanInterrupt())
	{
		//Check if the flag is set.
		if(m_uConfigFlags.IsFlagSet(CF_Interrupt))
		{
			return true;
		}
	}

	return false;
}

bool CTaskShove::ShouldProcessContacts() const
{
	//Ensure the game is not paused.
	if(fwTimer::IsGamePaused())
	{
		return false;
	}

	//Ensure we have not made contact.
	if(m_uRunningFlags.IsFlagSet(RF_HasMadeContact))
	{
		return false;
	}

	//Ensure the clip helper is valid.
	if(!GetClipHelper())
	{
		return false;
	}

	//Ensure the event is valid.
	if(!GetClipHelper()->IsEvent(CClipEventTags::MeleeCollision))
	{
		return false;
	}

	return true;
}

void CTaskShove::CleanUp()
{

}

aiTask* CTaskShove::Copy() const
{
	//Create the task.
	CTaskShove* pTask = rage_new CTaskShove(m_pTarget, m_uConfigFlags);

	return pTask;
}

bool CTaskShove::ProcessPostPreRender()
{
	//Check if we should process contacts.
	if(ShouldProcessContacts())
	{
		//Process contacts.
		ProcessContacts();
	}

	return CTask::ProcessPostPreRender();
}

CTask::FSM_Return CTaskShove::ProcessPreFSM()
{
	//Ensure the target is valid.
	if(!m_pTarget)
	{
		return FSM_Quit;
	}

	GetPed()->SetPedResetFlag(CPED_RESET_FLAG_ProcessPostPreRender, true);

	return FSM_Continue;
}

CTask::FSM_Return CTaskShove::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin
	
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_Shove)
			FSM_OnEnter
				Shove_OnEnter();
			FSM_OnUpdate
				return Shove_OnUpdate();
			FSM_OnExit
				Shove_OnExit();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskShove::Start_OnUpdate()
{
	//Assert that the task is valid.
	taskAssert(IsValid(*GetPed(), *m_pTarget));

	//Shove the target.
	SetState(State_Shove);

	return FSM_Continue;
}

void CTaskShove::Shove_OnEnter()
{
	//Choose the clip.
	fwMvClipId clipId = ChooseClip();
	taskAssert(clipId != CLIP_ID_INVALID);

	//Play the clip.
	StartClipBySetAndClip(GetPed(), CLIP_SET_REACTION_SHOVE, clipId, NORMAL_BLEND_IN_DELTA, CClipHelper::TerminationType_OnFinish);

	//Emit a shocking event for the confrontation.
	CEventShockingSeenConfrontation eventShocking(*GetPed(), m_pTarget);
	CShockingEventsManager::Add(eventShocking);
}

CTask::FSM_Return CTaskShove::Shove_OnUpdate()
{
	//Check if the clip has finished.
	if(!GetClipHelper() || GetClipHelper()->IsHeldAtEnd())
	{
		//Finish the task.
		SetState(State_Finish);
	}
	else if(ShouldInterrupt())
	{
		//Finish the task.
		SetState(State_Finish);
	}

	return FSM_Continue;
}

void CTaskShove::Shove_OnExit()
{
	//Stop the clip.
	StopClip(REALLY_SLOW_BLEND_OUT_DELTA);
}
