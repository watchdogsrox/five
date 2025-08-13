// Filename   :	TaskBlendFromNM.h
// Description:	Task to handle return to non-natural motion state.
//
// TODO RA: This is still only a first pass at FSMing this task. The states aren't
// very intuitive yet but it works.

#ifndef INC_TASKBLENDFROMNM_H_
#define INC_TASKBLENDFROMNM_H_

// --- Include Files ------------------------------------------------------------

// C headers
// Game headers
#include "fwanimation/posematcher.h"
#include "Task/System/Task.h"
#include "Task/System/TaskTypes.h"
#include "Task/Physics/TaskNM.h"
#include "Task/Physics/BlendFromNmData.h"

// ------------------------------------------------------------------------------

class CTaskAimGunScripted;

class CTaskBlendFromNM : public CTask
{
public:

	enum
	{
		State_Start = 0,
		State_DoingBlendOut,
		State_Finish
	};

	// Standard task blend from nm constructor. The task will determine which nm blend out set to use	
	CTaskBlendFromNM();

	// Forces the task to use a specific nm blend out set (if in doubt use the default constructor)
	CTaskBlendFromNM(eNmBlendOutSet forceBlendOutSet);

	~CTaskBlendFromNM();

	// Helper function for the constructors.
	void Init();

	virtual aiTask* Copy() const
	{
		return rage_new CTaskBlendFromNM();
	}

	////////////////////////////
	// CTask functions:
	virtual bool IsBlendFromNMTask() const	{ return true; }

protected:
	FSM_Return ProcessPreFSM();
	FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	s32 GetDefaultStateAfterAbort() const {return State_Finish;}

	void CleanUp();

	// Determine whether or not the task should abort.
	virtual bool ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent);
	// Handle aborting the task - this will be called in addition to CleanUp when we are aborting.
	virtual void DoAbort(const AbortPriority iPriority, const aiEvent* pEvent);

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		Assert(iState>=State_Start&&iState<=State_Finish);
		static const char* aStateNames[] = 
		{
			"State_Start",
			"State_DoingBlendOut",
			"State_Finish"
		};

		return aStateNames[iState];
	}
#endif // !__FINAL

private:
	//////////////////////////////////////////////
	// Helper functions for FSM state management:
	FSM_Return Start_OnUpdate(CPed* pPed);

	void       DoingBlendOut_OnEnter(CPed* pPed);
	FSM_Return DoingBlendOut_OnUpdate(CPed* pPed);

	FSM_Return Finish_OnUpdate();

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_BLEND_FROM_NM; }

	// Helper functions:

	void DoDirectBlendOut();

public:

	bool GetHasAborted() {return m_bHasAborted;}

	void SetRunningAsMotionTask(bool bIsMotionTask) { m_bRunningAsMotionTask=bIsMotionTask;}

	void MaintainDamageEntity(bool bMaintainDamageEntity) { m_bMaintainDamageEntity=bMaintainDamageEntity;}

	// Set by nm control when coming out of a balance behaviour (shouldn't do bumped reactions from things like shots or falls)
	void SetAllowBumpedReaction(bool bAllow) { m_bAllowBumpReaction = bAllow; }
	void SetBumpedByEntity(CEntity* pEnt) { m_pBumpedByEntity = pEnt; }

	// called by the collision event scanner to determine whether a balance or a roll up should be used when
	// the ped is bumped whilst doing a getup.
	bool ShouldRollupOnRagdollImpact() const { return false; }

	void SetForcedSetTarget(CAITarget& target)
	{
		m_forcedSetTarget = target;
	}

	void SetForcedSetMoveTask(CTask* pTask)
	{
		if (m_pForcedSetMoveTask)
		{
			delete m_pForcedSetMoveTask;
			m_pForcedSetMoveTask = NULL;
		}

		m_pForcedSetMoveTask = pTask;
	}

public:

	bool m_bHasAborted;
	bool m_bClearAllBehaviours;
	bool m_bRunningAsMotionTask;
	bool m_bResetDesiredHeadingOnSwitchToAnimated;
	bool m_bMaintainDamageEntity;

	RegdPhy m_pAttachToPhysical;
	Vector3 m_vecAttachOffset;

	bool m_bAllowBumpReaction;
	RegdEnt m_pBumpedByEntity;

	eNmBlendOutSet  m_forcedSet;
	CAITarget		m_forcedSetTarget;
	RegdTask		m_pForcedSetMoveTask;
};

#endif // ! INC_TASKBLENDFROMNM_H_
