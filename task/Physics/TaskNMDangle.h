// Filename   :	TaskNMDangle.h
// Description:	Called from script to run an NM behavior for dangling upside down from a meat hook

#ifndef INC_TASKNMDANGLE_H_
#define INC_TASKNMDANGLE_H_

// --- Include Files ------------------------------------------------------------

// Game headers
#include "Task/System/Task.h"
#include "Task\System\TaskTypes.h"
#include "Task/Physics/TaskNM.h"

// ------------------------------------------------------------------------------

class CTaskNMDangle : public CTaskNMBehaviour
{
public:

	CTaskNMDangle(const Vector3 &currentHookPos);
	~CTaskNMDangle();
	////////////////////////
	// CTaskSimple functions
#if !__FINAL
	virtual atString GetName() const {return atString("NMDangle");}
#endif
	virtual aiTask* Copy() const {return rage_new CTaskNMDangle(m_CurrentHookPos);}
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_NM_DANGLE;}

	void SetGrabParams(CPed* pPed, bool bDoGrab, float fFrequency);

	// Update after physics timestep
	bool ProcessPhysics(float fTimeStep, int nTimeSlice);

	void UpdateHookConstraint(const Vector3& position, const Vector3& vecRotMinLimits, const Vector3& vecRotMaxLimits);

	////////////////////////
	// CTaskFSMClone functions
	virtual CTaskInfo* CreateQueriableState() const;

	///////////////////////////
	// CTaskNMBehaviour functions
protected:
	virtual void StartBehaviour(CPed* pPed);
	virtual void ControlBehaviour(CPed* pPed);
	virtual bool FinishConditions(CPed* pPed);

	Vector3 m_CurrentHookPos;
	Vector3 m_LastHookPos;
	Vector3 m_MinAngles;
	Vector3 m_MaxAngles;

	phConstraintHandle m_RotConstraintHandle;

	bool			m_bDoGrab;
	bool			m_FirstPhysicsFrame;
	float			m_fFrequency;
};

#endif // ! INC_TASKNMDANGLE_H_
