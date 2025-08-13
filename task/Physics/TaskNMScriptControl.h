// Filename   :	TaskNMScriptControl.h
// Description:	Natural Motion script control class (FSM version)

#ifndef INC_TASKNMSCRIPTCONTROL_H_
#define INC_TASKNMSCRIPTCONTROL_H_

// --- Include Files ------------------------------------------------------------

// Game headers
#include "Task/System/Task.h"
#include "Task\System\TaskTypes.h"
#include "Task/Physics/TaskNM.h"

// ------------------------------------------------------------------------------

class CTaskNMScriptControl : public CTaskNMBehaviour
{
public:
	CTaskNMScriptControl(u32 nMaxTime, /*bool bAbortIfInjured, bool bAbortIfDead,*/ bool bForceControl);
	~CTaskNMScriptControl();

	////////////////////////
	// CTaskSimple functions
#if !__FINAL
	virtual atString GetName() const {return atString("NMScriptCtrl");}
#endif
	virtual aiTask* Copy() const {return rage_new CTaskNMScriptControl(m_nMaxTime, /*m_bAbortIfInjured, m_bAbortIfDead,*/ m_bForceControl);}
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_NM_SCRIPT_CONTROL;}

	////////////////////////
	// CTaskFSMClone functions
	virtual CTaskInfo* CreateQueriableState() const;

	///////////////////////////
	// CTaskNMBehaviour functions
public:
	virtual void BehaviourFailure(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);
	virtual void BehaviourSuccess(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);
	virtual void BehaviourFinish(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);

	bool HasSucceeded(eNMStrings nFeedback);
	bool HasFailed(eNMStrings nFeedback);
	bool HasFinished(eNMStrings nFeedback);

protected:
	virtual void StartBehaviour(CPed* pPed);
	virtual bool FinishConditions(CPed* pPed);

private:
	//	bool m_bAbortIfInjured;
	//	bool m_bAbortIfDead;
	bool m_bForceControl;

	// This should automatically scale with the number of NM behaviour messages now. To minimise memory
	// footprint, use an array of 32-bit flags rather than 64-bit ones.
	u32 m_nSuccessState[NM_NUM_MESSAGES/32];
	u32 m_nFailureState[NM_NUM_MESSAGES/32];
	u32 m_nFinishState[NM_NUM_MESSAGES/32];
};

#endif // ! INC_TASKNMSCRIPTCONTROL_H_
