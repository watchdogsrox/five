// Filename   :	TaskNMElectrocute.h

#ifndef INC_TASKNMELECTROCUTE_H_
#define INC_TASKNMELECTROCUTE_H_

// --- Include Files ------------------------------------------------------------

// Game headers
#include "Task/System/Task.h"
#include "Task/System/TaskTypes.h"
#include "Task/Physics/TaskNM.h"

// ------------------------------------------------------------------------------

//
// Task info for CTaskNMElectrocute
//
class CClonedNMElectrocuteInfo : public CSerialisedFSMTaskInfo
{
public:
    CClonedNMElectrocuteInfo() {}
	~CClonedNMElectrocuteInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_NM_ELECTROCUTE;}

	// the clone task is created by the CTaskNMControl clone task
	virtual bool AutoCreateCloneTask(CPed* UNUSED_PARAM(pPed)) { return false; }

	virtual CTaskFSMClone *CreateCloneFSMTask();

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);
	}

private:

	CClonedNMElectrocuteInfo(const CClonedNMElectrocuteInfo &);
	CClonedNMElectrocuteInfo &operator=(const CClonedNMElectrocuteInfo &);
};

//
// Task CTaskNMElectrocute
//
class CTaskNMElectrocute : public CTaskNMBehaviour
{
public:
	CTaskNMElectrocute(u32 nMinTime, u32 nMaxTime);
	CTaskNMElectrocute(u32 nMinTime, u32 nMaxTime, const Vector3& contactPosition);
	~CTaskNMElectrocute();

	struct Tunables : CTuning
	{
		Tunables();

		RagdollComponent m_InitialForceComponent;
		Vector3			 m_InitialForceOffset;
		CTaskNMBehaviour::Tunables::TunableForce m_InitialForce;

		float m_FallingSpeedForHighFall;

		CNmTuningSet m_Start;
		CNmTuningSet m_Walking;
		CNmTuningSet m_Running;
		CNmTuningSet m_Sprinting;

		CNmTuningSet m_OnElectrocuteFinished;
		CNmTuningSet m_OnBalanceFailed;
		CNmTuningSet m_OnCatchFallSuccess;

		PAR_PARSABLE;
	};

	////////////////////////
	// CTaskSimple functions
#if !__FINAL
	virtual atString GetName() const {return atString("CTaskNMElectrocute");}
#endif // !__FINAL
	virtual aiTask* Copy() const 
	{
		if (m_ContactPositionSpecified)
			return rage_new CTaskNMElectrocute(m_nMinTime, m_nMaxTime, m_ContactPosition);
		else
			return rage_new CTaskNMElectrocute(m_nMinTime, m_nMaxTime);
	}
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_NM_ELECTROCUTE;}

	////////////////////////
	// CTaskFSMClone functions
	virtual CTaskInfo* CreateQueriableState() const;

	///////////////////////////
	// CTaskNMBehaviour functions
public:
	virtual void BehaviourSuccess(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);
	virtual void BehaviourFailure(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);
protected:
	virtual void StartBehaviour(CPed* pPed);
	virtual void ControlBehaviour(CPed* pPed);
	virtual bool FinishConditions(CPed* pPed);

	void CreateAnimatedFallbackTask(CPed* pPed, bool bFalling);
	virtual void StartAnimatedFallback(CPed* pPed);
	virtual bool ControlAnimatedFallback(CPed* pPed);

public:
	static Tunables sm_Tunables;

private:
	bool m_ElectrocuteFinished;
	bool m_ContactPositionSpecified;
	bool m_ApplyInitialForce;
	Vector3 m_ContactPosition;

};

#endif // ! INC_TASKNMELECTROCUTE_H_
