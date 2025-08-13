// Filename   :	TaskNMGenericAttach.h
// Description:	

#ifndef INC_TASKNMGENERICATTACH_H_
#define INC_TASKNMGENERICATTACH_H_

// --- Include Files ------------------------------------------------------------

#include "Task/Physics/GrabHelper.h"
#include "Task/Physics/TaskNM.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/TaskTypes.h"

// ------------------------------------------------------------------------------

class CTaskNMGenericAttach : public CTaskNMBehaviour
{
public:
	CTaskNMGenericAttach(u32 nAttachFlags);
	~CTaskNMGenericAttach();

	/////////////////////////
	// CTaskSimple functions
#if !__FINAL
	virtual atString GetName() const {return atString("NMGenericAttach");}
#endif
	virtual aiTask* Copy() const;
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_NM_GENERIC_ATTACH;}

	////////////////////////
	// CTaskFSMClone functions
	virtual CTaskInfo* CreateQueriableState() const;

	///////////////////////////
	// CTaskNMBehaviour functions
public:
	virtual void BehaviourFailure(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);
	virtual void BehaviourSuccess(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);
protected:
	// Determine whether or not the task should abort.
	virtual bool ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent);

	virtual void StartBehaviour(CPed* pPed);
	virtual void ControlBehaviour(CPed* pPed);
	virtual bool FinishConditions(CPed* pPed);

	///////////////////////////
	// CONSTRAINT MANAGEMENT:
public:
	void DestroyAllConstraints(CPed* pPed);
	void UpdateConstraintFlags(CEntity* pEntity);


	//////////////////////////
	// Local functions and data
private:
	u32 m_nAttachFlags;
	bool m_bPedAttached;

#if __BANK
public:
#endif // __BANK
	static float sf_muscleStiffnessLeftArm;
	static float sf_muscleStiffnessRightArm;
	static float sf_muscleStiffnessSpine;
	static float sf_muscleStiffnessLeftLeg;
	static float sf_muscleStiffnessRightLeg;
	static float sf_stiffnessLeftArm;
	static float sf_stiffnessRightArm;
	static float sf_stiffnessSpine;
	static float sf_stiffnessLeftLeg;
	static float sf_stiffnessRightLeg;
	static float sf_dampingLeftArm;
	static float sf_dampingRightArm;
	static float sf_dampingSpine;
	static float sf_dampingLeftLeg;
	static float sf_dampingRightLeg;

};

#endif // !INC_TASKNMGENERICATTACH_H_
