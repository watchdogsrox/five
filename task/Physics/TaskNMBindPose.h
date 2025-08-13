// Filename   :	TaskNMBindPose.h
// Description:	Task to place a ped in the clip bind pose and suspend physics so that the transition
//              to and from natural motion can be inspected.

#ifndef INC_TASKNMBINDPOSE_H_
#define INC_TASKNMBINDPOSE_H_

#if __DEV

// --- Include Files ------------------------------------------------------------

#include "Task/System/TaskTypes.h"
#include "Task/Physics/TaskNM.h"

// ------------------------------------------------------------------------------

// Forward declaration:
class CTaskNMBindPose;

class CTaskBindPose : public CTask
{
public:
	// FSM states
	enum
	{
		State_Start = 0,
		State_StreamingClips,
		State_NmBindPose,
		State_ClipBindPose,
		State_Finish
	};

	// Constructor/destructor
	CTaskBindPose(bool bDoBlendFromNM);
	~CTaskBindPose() {}

	// Task required functions
	virtual aiTask* Copy() const;
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_BIND_POSE; }

	// FSM implementations
protected:
	virtual s32 GetDefaultStateAfterAbort() const { return State_Finish; }
	FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	// FSM Optional functions
	virtual	void CleanUp(); // Generic cleanup function, called when the task quits itself or is aborted.

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		Assert(iState>=State_Start&&iState<=State_Finish);
		static const char* aStateNames[] = 
		{
			"State_Start",
			"State_StreamingAnims",
			"State_NmBindPose",
			"State_AnimBindPose",
			"State_Finish"
		};

		return aStateNames[iState];
	}
#endif // !__FINAL

	// FSM State helper functions:
private:
	void       Start_OnEnter(CPed* pPed);
	FSM_Return Start_OnUpdate(CPed* pPed);
	void       StreamingClips_OnEnter(CPed* pPed);
	FSM_Return StreamingClips_OnUpdate(CPed* pPed);
	void       StreamingClips_OnExit(CPed* pPed);
	void       NmBindPose_OnEnter(CPed* pPed);
	FSM_Return NmBindPose_OnUpdate(CPed* pPed);
	void       NmBindPose_OnExit(CPed* pPed);
	void       ClipBindPose_OnEnter(CPed* pPed);
	FSM_Return ClipBindPose_OnUpdate(CPed* pPed);
	void       ClipBindPose_OnExit(CPed* pPed);

	// Public functions:
public:
	// Switch between animated and natural motion controlled bind pose.
	void ToggleNmBindPose();

	// Data:
private:
	CClipRequestHelper m_ClipStreamer;
	strRequest m_request; // Streaming request "smart pointer".

public:
	bool m_bDoBlendFromNM;
private:
	bool m_bSwitchStateSignal;
	bool m_bInNmMode;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CTaskNMBindPose : public CTaskNMBehaviour
{
public:
	CTaskNMBindPose(bool bDoBlendFromNM);
	~CTaskNMBindPose();

	/////////////////////////
	// CTaskSimple functions
#if !__FINAL
	virtual atString GetName() const {return atString("NMBindPose");}
#endif
	virtual aiTask* Copy() const;
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_NM_BIND_POSE;}

	////////////////////////
	// CTaskFSMClone functions
	virtual CTaskInfo* CreateQueriableState() const;

	///////////////////////////
	// CTaskNMBehaviour functions
protected:
	virtual void StartBehaviour(CPed* pPed);
	void ControlBehaviour(CPed *pPed);
	virtual bool FinishConditions(CPed* pPed);

	//////////////////////////
	// Local functions and data

public:
	void ExitTask();

private:
	bool m_bDoBlendFromNM;
	bool m_bExitSignalReceived; // Allow this task to be toggled on or off since there is no timeout.
};

#endif	//	__DEV

#endif // !INC_TASKNMBINDPOSE_H_
