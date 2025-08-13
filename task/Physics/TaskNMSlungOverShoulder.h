// Filename   :	TaskNMSlungOverShoulder.h
// Description:	Called from script to run an NM behavior for dangling upside down from a meat hook

#ifndef INC_TASKNMSLUNG_OVER_SHOULDER_H_
#define INC_TASKNMSLUNG_OVER_SHOULDER_H_

// --- Include Files ------------------------------------------------------------

// Game headers
#include "Task/System/Task.h"
#include "Task\System\TaskTypes.h"
#include "Task/Physics/TaskNM.h"

// ------------------------------------------------------------------------------

class CTaskNMSlungOverShoulder : public CTaskNMBehaviour
{
public:

	CTaskNMSlungOverShoulder(CPed *slungPed, CPed *carrierPed);
	~CTaskNMSlungOverShoulder();
	////////////////////////
	// CTaskSimple functions
#if !__FINAL
	virtual atString GetName() const {return atString("NMSlungOverShoulder");}
#endif
	virtual aiTask* Copy() const {return rage_new CTaskNMSlungOverShoulder(m_slungPed, m_carrierPed);}
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_NM_SLUNG_OVER_SHOULDER;}

	virtual FSM_Return ProcessPreFSM();

	// Update after physics timestep
	bool ProcessPhysics(float fTimeStep, int nTimeSlice);
	bool ProcessPostMovement();

	CPed * GetCarrierPed() const { return m_carrierPed; }
	CPed * GetSlungPed() const { return m_slungPed; }

	////////////////////////
	// CTaskFSMClone functions
	virtual CTaskInfo* CreateQueriableState() const;

	///////////////////////////
	// CTaskNMBehaviour functions
protected:
	virtual void StartBehaviour(CPed* pPed);
	virtual void ControlBehaviour(CPed* pPed);
	virtual bool FinishConditions(CPed* pPed);

	Matrix34 m_offset1;

	bool m_bCarryClipsLoaded;

	CPed *m_slungPed;
	CPed *m_carrierPed;

	phConstraintHandle m_Handle;
};

#endif // ! INC_TASKNMSLUNG_OVER_SHOULDER_H_
