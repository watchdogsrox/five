// Filename   :	TaskNMBuoyancy.h
// Description:	GTA-side of the NM Buoyancy behavior

#ifndef INC_TASKNMBUOYANCY_H_
#define INC_TASKNMBUOYANCY_H_

// --- Include Files ------------------------------------------------------------

// Game headers
#include "Task/System/Task.h"
#include "Task\System\TaskTypes.h"
#include "Task/Physics/TaskNM.h"

// ------------------------------------------------------------------------------

//
// Task info for CTaskNMBuoyancy
//
class CClonedNMBuoyancyInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedNMBuoyancyInfo() {}
	~CClonedNMBuoyancyInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_NM_BUOYANCY;}

	// the clone task is created by the CTaskNMControl clone task
	virtual bool AutoCreateCloneTask(CPed* UNUSED_PARAM(pPed)) { return false; }

	virtual CTaskFSMClone *CreateCloneFSMTask();

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);
	}

private:

	CClonedNMBuoyancyInfo(const CClonedNMBuoyancyInfo &);
	CClonedNMBuoyancyInfo &operator=(const CClonedNMBuoyancyInfo &);
};


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskNMBuoyancy //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CTaskNMBuoyancy : public CTaskNMBehaviour
{
public:

	struct Tunables : CTuning
	{
		Tunables();

		CTaskNMBehaviour::Tunables::BlendOutThreshold m_BlendOutThreshold;

		PAR_PARSABLE;
	};

	CTaskNMBuoyancy(u32 nMinTime, u32 nMaxTime);
	~CTaskNMBuoyancy();
	////////////////////////
	// CTaskSimple functions
#if !__FINAL
	virtual atString GetName() const {return m_strTaskName;}
#endif
	virtual aiTask* Copy() const {return rage_new CTaskNMBuoyancy(m_nMinTime, m_nMaxTime);}
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_NM_BUOYANCY;}

	///////////////////////////
	// CTaskNMBehaviour functions
protected:
	virtual void StartBehaviour(CPed* pPed);
	virtual void ControlBehaviour(CPed* pPed);
	virtual bool FinishConditions(CPed* pPed);

	////////////////////////
	// CTaskFSMClone functions
	virtual CTaskInfo* CreateQueriableState() const;

	//////////////////////////
	// Local functions and data
private:

#if !__FINAL
	// For debugging the internal state machine.
	atString m_strTaskName;
#endif // !__FINAL

public:
	static Tunables	sm_Tunables;
};


#endif // INC_TASKNMBUOYANCY_H_