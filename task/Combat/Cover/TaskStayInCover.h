// Title	:	TaskSeekCover.h
// Author	:	Phil Hooker
// Started	:	31/05/05
// This selection of class' enables individual peds to seek and hide in cover

#ifndef TASK_STAY_IN_COVER_H
#define TASK_STAY_IN_COVER_H

#include "Task/System/TaskTypes.h"
#include "Scene/RefMacros.h"
#include "Scene/Entity.h"

class CTaskStayInCover : public CTask
{
public:

	typedef enum
	{
		State_Invalid = -1,
		State_Start = 0,
		State_StandAndAimInArea,
		State_SearchForCoverInArea,
		State_GoToDefensiveArea,
		State_InCoverInArea,
		State_Finish
	} SearchState;

	// Constructor/destructor
	CTaskStayInCover();
	~CTaskStayInCover();

	// Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();
	virtual aiTask* Copy() const { return rage_new CTaskStayInCover(); }
	virtual s32 GetTaskTypeInternal() const {return CTaskTypes::TASK_STAY_IN_COVER;}

	// FSM implementations
	FSM_Return		UpdateFSM			(const s32 iState, const FSM_Event iEvent);
	FSM_Return		ProcessPreFSM		();

	virtual s32	GetDefaultStateAfterAbort()	const {return State_SearchForCoverInArea;}

	// FSM state implementations
	// Start
	FSM_Return Start_OnUpdate(CPed* pPed);
	// StandAndAimInArea
	FSM_Return StandAndAimInArea_OnEnter(CPed* pPed);
	FSM_Return StandAndAimInArea_OnUpdate(CPed* pPed);
	// GoToDefensiveArea
	FSM_Return GoToDefensiveArea_OnEnter(CPed* pPed);
	FSM_Return GoToDefensiveArea_OnUpdate(CPed* pPed);
	// SearchForCoverInArea
	FSM_Return SearchForCoverInArea_OnEnter(CPed* pPed);
	FSM_Return SearchForCoverInArea_OnUpdate(CPed* pPed);
	// SearchForCoverInArea
	void InCoverInArea_OnEnter(CPed* UNUSED_PARAM(pPed));
	FSM_Return InCoverInArea_OnUpdate(CPed* pPed);
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

	virtual CTaskInfo*	CreateQueriableState() const;

private:
	// Helper functions
	void CalculateAimAtPosition(CPed* pPed, Vector3& vTargetPosition, Vector3& vCoverSearchStartPosition);

	CTaskTimer	m_goToAreaTimer;

	float m_fCoverSearchTimer;
	bool  m_bIsInDefensiveArea;
	bool  m_bCanUseGunTask;
};

class CClonedStayInCoverInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedStayInCoverInfo() {}
	CClonedStayInCoverInfo(s32 state)
	{ 
		SetStatusFromMainTaskState(state); 
	}	
	~CClonedStayInCoverInfo() {}

	virtual s32 GetTaskInfoType() const {return INFO_TYPE_STAY_IN_COVER;}
	virtual int GetScriptedTaskType() const { return SCRIPT_TASK_STAY_IN_COVER; }

	virtual bool    HasState() const { return true; }
	virtual u32		GetSizeOfState() const { return datBitsNeeded<CTaskStayInCover::State_Finish>::COUNT;}
	virtual const char*	GetStatusName() const { return "State"; }

	virtual CTask *CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity));
};


#endif // TASK_STAY_IN_COVER_H
