#ifndef TASK_REACH_ARM_H
#define TASK_REACH_ARM_H

//framework headers

// Game headers
#include "animation/AnimDefines.h"
#include "Task/System/Task.h"
#include "ik/IkManager.h"
#include "Ik/solvers/ArmIkSolver.h"


class CTaskReachArm : public CTask
{

public:

	// task states
	enum
	{
		State_Start,
		State_Running,
		State_Exit
	};

	// PURPOSE: Construct the task to reach for a global space point in the world.
	CTaskReachArm(crIKSolverArms::eArms arm, Vec3V_In worldPosition, s32 armIkFlags = AIK_TARGET_WRT_POINTHELPER, s32 durationMs = -1, float blendInDuration = SLOW_BLEND_DURATION, float blendOutDuration = SLOW_BLEND_DURATION );

	// PURPOSE: Construct the task to reach for a point relative to an entity
	CTaskReachArm(crIKSolverArms::eArms arm,  CDynamicEntity* pEnt, Vec3V_In offset, s32 armIkFlags = AIK_TARGET_WRT_POINTHELPER, s32 durationMs = -1, float blendInDuration = SLOW_BLEND_DURATION, float blendOutDuration = SLOW_BLEND_DURATION );

	// PURPOSE: Construct the task to reach for a point relative to a bone on an entity
	CTaskReachArm(crIKSolverArms::eArms arm,  CDynamicEntity* pEnt, eAnimBoneTag boneId, Vec3V_In offset, s32 armIkFlags = AIK_TARGET_WRT_POINTHELPER, s32 durationMs = -1, float blendInDuration = SLOW_BLEND_DURATION, float blendOutDuration = SLOW_BLEND_DURATION );

	virtual ~CTaskReachArm()
	{

	}

	void SetArm(crIKSolverArms::eArms arm) { m_arm = arm; }
	void SetClip(fwMvClipSetId clipSet, fwMvClipId clip, fwMvFilterId filter);

	virtual aiTask* Copy() const { 
		CTaskReachArm* pNewTask = NULL;
		switch(m_targetType)
		{
		case Target_WorldPosition:
			pNewTask =  rage_new CTaskReachArm(m_arm, m_offset.d(), m_controlFlags, m_reachDurationInMs, m_blendInDuration, m_blendOutDuration);
			break;
		case Target_EntityOffset:
			pNewTask =  rage_new CTaskReachArm(m_arm, m_pTargetEntity, m_offset.d(), m_controlFlags, m_reachDurationInMs, m_blendInDuration, m_blendOutDuration);
			break;
		case Target_EntityBoneOffset:
			pNewTask =  rage_new CTaskReachArm(m_arm, m_pTargetEntity, m_boneId, m_offset.d(), m_controlFlags, m_reachDurationInMs, m_blendInDuration, m_blendOutDuration);
			break;
		default:
			taskAssertf(0, "Unhandled target type!");
		}

		if (pNewTask)
		{
			pNewTask->SetClip( m_clipSetId, m_clipId, m_clipFilter );
		}

		return pNewTask;
	}

	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_REACH_ARM; }
	virtual s32				GetDefaultStateAfterAbort()	const {return State_Exit;}

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

	virtual FSM_Return ProcessPreFSM();
	FSM_Return		UpdateFSM			(const s32 iState, const FSM_Event iEvent);	
	void			CleanUp			();

private:

	enum eTargetType
	{
		Target_WorldPosition = 0,
		Target_EntityOffset,
		Target_EntityBoneOffset
	};

		// State Functions
	// Start
	void						Start_OnEnter();
	FSM_Return					Start_OnUpdate();

	void						Running_OnEnter();
	FSM_Return					Running_OnUpdate();
	void						Running_OnExit();

	Mat34V					m_offset;
	RegdDyn					m_pTargetEntity;
	eAnimBoneTag			m_boneId;
	s32						m_controlFlags;
	s32						m_reachDurationInMs;
	float					m_blendInDuration;
	float					m_blendOutDuration;
	crIKSolverArms::eArms	m_arm;

	fwMvClipSetId			m_clipSetId;
	fwMvClipId				m_clipId;
	fwMvFilterId			m_clipFilter;

	// internal use variables
	eTargetType		m_targetType;
	s32				m_boneIndex;
	u32				m_startTime;
};

#endif // TASK_REACH_ARM_H
