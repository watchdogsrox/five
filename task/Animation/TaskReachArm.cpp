
#include "TaskReachArm.h"

//	rage includes

//	game includes
#include "peds/Ped.h"
#include "ik/IkManager.h"


AI_OPTIMISATIONS()

// PURPOSE: Construct the task to reach for a global space point in the world.
CTaskReachArm::CTaskReachArm(crIKSolverArms::eArms arm, Vec3V_In worldPosition, s32 armIkFlags, s32 durationMs, float blendInDuration, float blendOutDuration )
: m_pTargetEntity(NULL)
, m_boneId((eAnimBoneTag)0)
, m_controlFlags(armIkFlags)
, m_reachDurationInMs(durationMs)
, m_blendInDuration(blendInDuration)
, m_blendOutDuration(blendOutDuration)
, m_arm(arm)
, m_targetType(Target_WorldPosition)
, m_boneIndex(-1)
, m_clipSetId(CLIP_SET_ID_INVALID)
, m_clipId(CLIP_ID_INVALID)
, m_clipFilter(FILTER_ID_INVALID)
{
	m_offset.SetIdentity3x3();
	m_offset.Setd(worldPosition);
	SetInternalTaskType(CTaskTypes::TASK_REACH_ARM);
}

// PURPOSE: Construct the task to reach for a point relative to an entity
CTaskReachArm::CTaskReachArm(crIKSolverArms::eArms arm,  CDynamicEntity* pEnt, Vec3V_In offset, s32 armIkFlags , s32 durationMs, float blendInDuration, float blendOutDuration)
: m_pTargetEntity(pEnt)
, m_boneId((eAnimBoneTag)0)
, m_controlFlags(armIkFlags)
, m_reachDurationInMs(durationMs)
, m_blendInDuration(blendInDuration)
, m_blendOutDuration(blendOutDuration)
, m_arm(arm)
, m_targetType(Target_EntityOffset)
, m_boneIndex(-1)
, m_clipSetId(CLIP_SET_ID_INVALID)
, m_clipId(CLIP_ID_INVALID)
, m_clipFilter(FILTER_ID_INVALID)
{
	taskAssert(pEnt);
	m_offset.Setd(offset);
	SetInternalTaskType(CTaskTypes::TASK_REACH_ARM);
}
// PURPOSE: Construct the task to reach for a point relative to a bone on an entity
CTaskReachArm::CTaskReachArm(crIKSolverArms::eArms arm, CDynamicEntity* pEnt, eAnimBoneTag boneId, Vec3V_In offset, s32 armIkFlags, s32 durationMs, float blendInDuration, float blendOutDuration )
: m_offset(offset)
, m_pTargetEntity(pEnt)
, m_boneId(boneId)
, m_controlFlags(armIkFlags)
, m_reachDurationInMs(durationMs)
, m_blendInDuration(blendInDuration)
, m_blendOutDuration(blendOutDuration)
, m_arm(arm)
, m_targetType(Target_EntityBoneOffset)
, m_boneIndex(-1)
, m_clipSetId(CLIP_SET_ID_INVALID)
, m_clipId(CLIP_ID_INVALID)
, m_clipFilter(FILTER_ID_INVALID)
{
	taskAssert(pEnt);
	if (pEnt)
	{
		pEnt->GetSkeletonData().ConvertBoneIdToIndex((u16)m_boneId, m_boneIndex);
	}

	m_offset.Setd(offset);

	taskAssert(m_boneIndex>-1);
	SetInternalTaskType(CTaskTypes::TASK_REACH_ARM);
}

CTask::FSM_Return CTaskReachArm::ProcessPreFSM()
{
	return FSM_Continue;
}


CTask::FSM_Return CTaskReachArm::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin
		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter();
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_Running)
			FSM_OnEnter
				Running_OnEnter();
			FSM_OnUpdate
				return Running_OnUpdate();
			FSM_OnExit
				Running_OnExit();

		FSM_State(State_Exit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

//////////////////////////////////////////////////////////////////////////

void CTaskReachArm::SetClip(fwMvClipSetId clipSet, fwMvClipId clip, fwMvFilterId filter)
{
	m_clipSetId = clipSet;
	m_clipId = clip;
	m_clipFilter = filter;
}

//////////////////////////////////////////////////////////////////////////

void CTaskReachArm::Start_OnEnter()
{
	m_startTime = fwTimer::GetTimeInMilliseconds();

}



CTask::FSM_Return CTaskReachArm::Start_OnUpdate()
{
	SetState(State_Running);
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

void CTaskReachArm::Running_OnEnter()
{
	if (m_clipSetId!=CLIP_SET_ID_INVALID)
	{
		StartClipBySetAndClip(GetPed(), m_clipSetId, m_clipId, APF_ISLOOPED | APF_USE_SECONDARY_SLOT, m_clipFilter.GetHash(), AP_LOW, m_blendInDuration ? 1.0f/m_blendInDuration : INSTANT_BLEND_IN_DELTA, CClipHelper::TerminationType_OnDelete );
	}
}

CTask::FSM_Return CTaskReachArm::Running_OnUpdate()
{
	CPed* pPed = GetPed();
	taskAssert(pPed);

	// If we're dead, stop reaching
	if (pPed->IsDead())
	{
		SetState(State_Exit);
		return FSM_Continue;
	}
	
	// Is it time to exit?
	if ((m_reachDurationInMs > -1) && (fwTimer::GetTimeInMilliseconds()>(m_startTime+((u32)m_reachDurationInMs))))
	{
		SetState(State_Exit);
		return FSM_Continue;
	}

	// If we should be playing an anim, and the anim finishes, end the task
	if (m_clipSetId!=CLIP_SET_ID_INVALID && !GetClipHelper())
	{
		SetState(State_Exit);
		return FSM_Continue;
	}

	switch(m_targetType)
	{
	case Target_WorldPosition:
		{
			pPed->GetIkManager().SetAbsoluteArmIKTarget(m_arm, VEC3V_TO_VECTOR3(m_offset.d()), m_controlFlags, m_blendInDuration, m_blendOutDuration);
		}
		break;
	case Target_EntityOffset:
		{
			pPed->GetIkManager().SetRelativeArmIKTarget(m_arm, m_pTargetEntity, -1, VEC3V_TO_VECTOR3(m_offset.d()), m_controlFlags, m_blendInDuration, m_blendOutDuration);
		}
		break;
	case Target_EntityBoneOffset:
		{
			pPed->GetIkManager().SetRelativeArmIKTarget(m_arm, m_pTargetEntity, m_boneIndex, VEC3V_TO_VECTOR3(m_offset.d()), m_controlFlags, m_blendInDuration, m_blendOutDuration);
		}
		break;
	default:
		{
			taskAssertf(0, "Unhandled target type!");
			SetState(State_Exit);
		}
		break;
	}
	
	return FSM_Continue;
}


void CTaskReachArm::Running_OnExit()
{
	StopClip(m_blendOutDuration ? -1.0f/m_blendOutDuration : INSTANT_BLEND_OUT_DELTA);
}

//////////////////////////////////////////////////////////////////////////

void CTaskReachArm::CleanUp()
{
	
}

#if !__FINAL
const char * CTaskReachArm::GetStaticStateName( s32 iState  )
{
	static char szStateName[128];
	switch (iState)
	{
	case State_Start:					sprintf(szStateName, "State_Start"); break;
	case State_Running:					sprintf(szStateName, "State_Running"); break;
	case State_Exit:					sprintf(szStateName, "State_Exit"); break;
	default: taskAssert(0); 	
	}
	return szStateName;
}
#endif // __FINAL
