// Filename   :	TaskNMRollUpAndRelax.h
// Description:	Natural Motion roll up and relax class (FSM version)

#ifndef INC_TASKNMROLLUPANDRELAX_H_
#define INC_TASKNMROLLUPANDRELAX_H_

// --- Include Files ------------------------------------------------------------

// Game headers
#include "Task/System/Task.h"
#include "Task\System\TaskTypes.h"
#include "Task/Physics/TaskNM.h"
#include "Task/Physics/TaskNMRelax.h"

// ------------------------------------------------------------------------------

//
// Task info for CTaskNMRollUpAndRelax
//
class CClonedNMRollUpAndRelaxInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedNMRollUpAndRelaxInfo(u32 nRollUpTime, float fStiffness, float fDamping);
	CClonedNMRollUpAndRelaxInfo();
	~CClonedNMRollUpAndRelaxInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_NM_ROLLUPANDRELAX;}

	// the clone task is created by the CTaskNMControl clone task
	virtual bool AutoCreateCloneTask(CPed* UNUSED_PARAM(pPed)) { return false; }

	virtual CTaskFSMClone *CreateCloneFSMTask();

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		SERIALISE_UNSIGNED(serialiser, m_nRollUpTime, SIZEOF_ROLLUPTIME, "Roll up time");

		SERIALISE_BOOL(serialiser, m_bHasStiffness, "Has stiffness");

		if (m_bHasStiffness || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fStiffness, 30.0f, SIZEOF_STIFFNESS, "Stiffness");
		}

		SERIALISE_BOOL(serialiser, m_bHasDamping, "Has damping");

		if (m_bHasDamping || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fDamping, 30.0f, SIZEOF_DAMPING, "Damping");
		}
	}

private:

	CClonedNMRollUpAndRelaxInfo(const CClonedNMRollUpAndRelaxInfo &);
	CClonedNMRollUpAndRelaxInfo &operator=(const CClonedNMRollUpAndRelaxInfo &);

	static const unsigned int SIZEOF_ROLLUPTIME			= 13;
	static const unsigned int SIZEOF_STIFFNESS			= 16;
	static const unsigned int SIZEOF_DAMPING			= 16;

	u32		m_nRollUpTime;
	float		m_fStiffness;
	float		m_fDamping;
	bool		m_bHasStiffness;
	bool		m_bHasDamping;
};

//
// Task CTaskNMRollUpAndRelax
//
class CTaskNMRollUpAndRelax : public CTaskNMRelax
{
public:
	CTaskNMRollUpAndRelax(u32 nMinTime, u32 nMaxTime, u32 m_nRollUpTime, float fStiffness = -1.0f, float fDamping = -1.0f);
	~CTaskNMRollUpAndRelax();

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_NM_ROLL_UP_AND_RELAX;}

	//////////////////////////////
	// CTaskNMBehaviour functions
	virtual aiTask* Copy() const {return rage_new CTaskNMRollUpAndRelax(m_nMinTime, m_nMaxTime, m_nRollUpTime, m_fStiffness, m_fDamping);}

	////////////////////////
	// CTaskFSMClone functions
	virtual CTaskInfo* CreateQueriableState() const
	{
		return rage_new CClonedNMRollUpAndRelaxInfo(m_nRollUpTime, m_fStiffness, m_fDamping);
	}

	//////////////////////////////
	// CTaskNMBehaviour functions
protected:
	virtual void StartBehaviour(CPed* pPed);
	virtual void ControlBehaviour(CPed* pPed);
	virtual bool FinishConditions(CPed* pPed);

protected:

	const u32 m_nRollUpTime;
	bool bIsRelaxing;
};


#endif // ! INC_TASKNMROLLUPANDRELAX_H_
